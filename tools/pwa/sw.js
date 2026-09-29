/* Service worker do controle mobile.
 * Guarda so' a "casca" do app (pagina, manifest, icones) para ele abrir mesmo
 * sem rede -- nesse caso a pagina mostra "sem conexao". Estado e comandos
 * (/events, /cmd, /auth) sempre vao direto para o servidor, nunca do cache.
 * Rede primeiro: quando o servidor responde, a versao nova substitui a antiga. */
const CACHE = "amv-v1";
const CASCA = ["/m", "/manifest.webmanifest", "/pwa/icon-192.png", "/pwa/icon-512.png"];

self.addEventListener("install", (e) => {
  e.waitUntil(caches.open(CACHE).then((c) => c.addAll(CASCA)).then(() => self.skipWaiting()));
});

self.addEventListener("activate", (e) => {
  e.waitUntil(
    caches.keys()
      .then((nomes) => Promise.all(nomes.filter((n) => n !== CACHE).map((n) => caches.delete(n))))
      .then(() => self.clients.claim())
  );
});

self.addEventListener("fetch", (e) => {
  const url = new URL(e.request.url);
  if (e.request.method !== "GET" || !CASCA.includes(url.pathname)) return;  // passa direto
  e.respondWith(
    fetch(e.request)
      .then((r) => {
        const copia = r.clone();
        caches.open(CACHE).then((c) => c.put(url.pathname, copia));
        return r;
      })
      .catch(() => caches.match(url.pathname))
  );
});
