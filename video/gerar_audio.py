"""
Gera o áudio do vídeo, tudo local:
  - narração (voz masculina pt-BR "faber" do Piper) -> public/voz/<cena>.wav
  - trilha de fundo sintetizada aqui mesmo        -> public/musica.wav
  - public/voz/duracoes.json (segundos de cada fala, usado para abaixar a música)

  python3 gerar_audio.py            (instala as deps: pip install --target .pylib piper-tts numpy)

Cada fala tem que caber na cena: se passar do limite, a fala é acelerada
(length_scale) até 0,85; se ainda não couber, o script avisa para encurtar o texto.
"""
import json
import subprocess
import sys
import wave
from pathlib import Path

AQUI = Path(__file__).resolve().parent
sys.path.insert(0, str(AQUI / ".pylib"))
import numpy as np  # noqa: E402

VOZ = AQUI / ".vozes/pt_BR-faber-medium.onnx"
SAIDA = AQUI / "public/voz"
SR = 44100

# Duração das cenas (s) — igual a src/config.ts. A fala começa 0,35 s após o
# início da cena e tem que terminar 0,3 s antes do fim.
CENAS = {"titulo": 4, "boot": 5, "manobra": 6, "ramal": 8.5, "espera": 5,
         "emerg": 8.5, "obstr": 4.5, "frenagem": 9, "numeros": 7, "fim": 3.5}
ATRASO, FOLGA = 0.35, 0.3


def dur_wav(p):
    with wave.open(str(p)) as w:
        return w.getnframes() / w.getframerate()


def falar(texto, destino, escala):
    env = {"PYTHONPATH": str(AQUI / ".pylib"), "PATH": "/usr/bin:/bin"}
    subprocess.run([sys.executable, "-m", "piper", "-m", str(VOZ), "-f", str(destino),
                    "--length-scale", f"{escala:.3f}", "--sentence-silence", "0.15"],
                   input=texto.encode(), env=env, check=True, capture_output=True)


def narracao():
    SAIDA.mkdir(parents=True, exist_ok=True)
    textos = json.loads((AQUI / "src/narracao.json").read_text())
    duracoes = {}
    for cena, texto in textos.items():
        limite = CENAS[cena] - ATRASO - FOLGA
        destino = SAIDA / f"{cena}.wav"
        escala = 1.0
        while True:
            falar(texto, destino, escala)
            d = dur_wav(destino)
            if d <= limite or escala <= 0.85:
                break
            escala = max(0.85, escala * limite / d * 0.98)
        aviso = "  <-- NÃO CABE, encurte o texto" if d > limite else ""
        print(f"{cena:9s} {d:5.2f}s / {limite:4.2f}s  (escala {escala:.2f}){aviso}")
        duracoes[cena] = round(d, 3)
    (SAIDA / "duracoes.json").write_text(json.dumps(duracoes, indent=2))


# ------------------------------------------------------------------ música
def nota(n):  # número MIDI -> Hz
    return 440.0 * 2 ** ((n - 69) / 12)


def musica(total=60.0, bpm=92):
    t_beat = 60 / bpm
    n = int(total * SR)
    L = np.zeros(n)
    R = np.zeros(n)
    rng = np.random.default_rng(7)

    # Am - F - C - G, um acorde por compasso (4 tempos)
    acordes = [[57, 60, 64], [53, 57, 60], [48, 55, 60], [55, 59, 62]]
    baixos = [45, 41, 48, 43]
    compasso = 4 * t_beat

    def env_adsr(m, a, r):
        e = np.ones(m)
        ia, ir = int(a * SR), int(r * SR)
        e[:ia] = np.linspace(0, 1, ia)
        e[-ir:] *= np.linspace(1, 0, ir)
        return e

    def soma(sinal, ini, ganho, pan=0.0):
        i0 = int(ini * SR)
        if i0 >= n:
            return
        s = sinal[: n - i0]
        L[i0:i0 + len(s)] += s * ganho * (1 - pan) / 2 * 2 ** 0.5
        R[i0:i0 + len(s)] += s * ganho * (1 + pan) / 2 * 2 ** 0.5

    k = 0
    t = 0.0
    while t < total:
        ac = acordes[k % 4]
        m = int((compasso + 0.6) * SR)
        tt = np.arange(m) / SR
        # pad: 3 notas, levemente desafinadas, poucos harmônicos
        pad = np.zeros(m)
        for nn in ac:
            for det in (-0.12, 0.12):
                f = nota(nn + 12) * 2 ** (det / 12)
                pad += np.sin(2 * np.pi * f * tt) + 0.25 * np.sin(4 * np.pi * f * tt)
        pad *= env_adsr(m, 0.5, 0.8) / 6
        soma(pad, t, 0.22, pan=-0.2 if k % 2 else 0.2)
        # baixo
        b = np.sin(2 * np.pi * nota(baixos[k % 4]) * tt) * env_adsr(m, 0.02, 0.4)
        soma(b, t, 0.25)
        # arpejo em colcheias
        arp = ac + [ac[0] + 12, ac[1] + 12]
        for j in range(8):
            mm = int(0.9 * SR)
            ta = np.arange(mm) / SR
            f = nota(arp[(j * 3 + k) % len(arp)] + 12)
            s = (np.sin(2 * np.pi * f * ta) + 0.3 * np.sin(6 * np.pi * f * ta)) * np.exp(-ta * 5)
            soma(s, t + j * t_beat / 2, 0.10, pan=0.5 if j % 2 else -0.5)
        # bumbo nos tempos 1 e 3, chimbal nas colcheias (entram após 2 compassos)
        if k >= 2:
            for j in range(4):
                if j % 2 == 0:
                    mm = int(0.35 * SR)
                    ta = np.arange(mm) / SR
                    f = 110 * np.exp(-ta * 18) + 45
                    kick = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-ta * 9)
                    soma(kick, t + j * t_beat, 0.45)
            for j in range(8):
                mm = int(0.05 * SR)
                ch = rng.standard_normal(mm) * np.exp(-np.arange(mm) / SR * 90)
                ch = np.diff(ch, prepend=0)  # tira o grave
                soma(ch, t + j * t_beat / 2, 0.035 if j % 2 else 0.05, pan=0.3)
        t += compasso
        k += 1

    est = np.stack([L, R], axis=1)
    # eco curto para dar espaço
    atraso = int(t_beat * 0.75 * SR)
    est[atraso:] += 0.25 * est[:-atraso][:, ::-1]
    # fade in/out
    fi, fo = int(1.0 * SR), int(3.0 * SR)
    est[:fi] *= np.linspace(0, 1, fi)[:, None]
    est[-fo:] *= np.linspace(1, 0, fo)[:, None]
    est /= np.max(np.abs(est)) / 0.8
    pcm = (est * 32767).astype("<i2")
    with wave.open(str(AQUI / "public/musica.wav"), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())
    print(f"musica.wav {total:.0f}s")


if __name__ == "__main__":
    narracao()
    musica()
