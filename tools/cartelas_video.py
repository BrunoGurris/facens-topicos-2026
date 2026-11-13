import sys
from PIL import Image, ImageDraw, ImageFont
OUT = sys.argv[1]
W, H = 1920, 1080
R = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
B = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
f = lambda p, s: ImageFont.truetype(p, s)
BG, FG, MUT, ACC = (24, 28, 34), (240, 240, 236), (160, 166, 175), (232, 176, 46)
RED, GRN, YEL = (214, 69, 65), (76, 175, 80), (232, 176, 46)

def signal(d, x, y, lit):
    d.rounded_rectangle([x, y, x + 70, y + 200], 18, fill=(40, 44, 52), outline=(90, 94, 102), width=3)
    for i, c in enumerate([RED, YEL, GRN]):
        on = c == lit
        d.ellipse([x + 13, y + 13 + i * 62, x + 57, y + 57 + i * 62], fill=c if on else (60, 62, 68))

def card(name, draw_fn):
    im = Image.new("RGB", (W, H), BG); d = ImageDraw.Draw(im); draw_fn(d); im.save(f"{OUT}/{name}.png")

def titulo(d):
    signal(d, 160, 380, GRN)
    d.text((300, 330), "Controle de AMV com FreeRTOS", font=f(B, 84), fill=FG)
    d.text((300, 450), "Aparelho de Mudança de Via (desvio ferroviário)", font=f(R, 46), fill=MUT)
    d.text((300, 540), "STM32 Nucleo-C031C6  ·  simulado no Wokwi", font=f(R, 40), fill=ACC)
    d.text((300, 900), "Tópicos Especiais II  ·  RTOS embarcados  ·  2026", font=f(R, 32), fill=MUT)

def frenagem(d):
    d.text((120, 90), "O trem obedece o sinal", font=f(B, 72), fill=FG)
    d.text((120, 190), "curva de frenagem calculada a cada 50 ms:  v = √(v₀² + 2·a·d)", font=f(R, 38), fill=MUT)
    rows = [("Sinal vermelho", "freia e para 5 m antes do sinal", RED),
            ("Sinal amarelo", "reduz para 30 km/h no ramal (medido: 29 km/h)", YEL),
            ("Sinal fecha tarde", "frenagem de emergência", RED)]
    y = 320
    for a, b, c in rows:
        d.ellipse([120, y + 8, 160, y + 48], fill=c)
        d.text((190, y), a, font=f(B, 44), fill=FG); d.text((640, y + 4), b, font=f(R, 40), fill=FG); y += 100
    y += 30
    d.line([120, y, 1800, y], fill=(70, 74, 82), width=2); y += 40
    d.text((120, y), "Frenagem de serviço  1,2 m/s²   →   parada a 80 km/h em ~206 m", font=f(R, 40), fill=FG); y += 70
    d.text((120, y), "Frenagem de emergência  2,5 m/s²   →   parada a 80 km/h em ~99 m", font=f(R, 40), fill=FG); y += 100
    d.text((120, y), "Teste a 80 km/h: nenhum avanço do vermelho, nenhum descarrilamento", font=f(B, 38), fill=ACC)

def numeros(d):
    d.text((120, 90), "Medido na placa × meta", font=f(B, 72), fill=FG)
    d.text((120, 190), "cronômetro de 1 µs (TIM14) no próprio STM32", font=f(R, 38), fill=MUT)
    rows = [("Emergência → sinal vermelho", "200 µs", "≤ 10 ms"),
            ("Perda de detecção → vermelho", "253 µs", "≤ 10 ms"),
            ("Pedido → motor comandado", "368 µs", "≤ 50 ms"),
            ("Manobra completa", "3,1 s", "≤ 4,5 s"),
            ("Jitter do motor", "556 µs", "≤ 1 ms")]
    y = 310
    d.text((1180, y - 70), "medido", font=f(R, 34), fill=MUT); d.text((1480, y - 70), "meta", font=f(R, 34), fill=MUT)
    for a, b, c in rows:
        d.text((120, y), a, font=f(R, 46), fill=FG)
        d.text((1180, y), b, font=f(B, 46), fill=ACC)
        d.text((1480, y), c, font=f(R, 46), fill=MUT)
        d.text((1740, y), "✓", font=f(B, 46), fill=GRN); y += 100
    d.text((120, y + 30), "7 tarefas · 5 filas · 1 mutex (herança de prioridade) · 7/7 testes automáticos", font=f(R, 36), fill=MUT)

def fim(d):
    signal(d, 160, 380, GRN)
    d.text((300, 380), "Sinal só abre com o AMV travado.", font=f(B, 70), fill=FG)
    d.text((300, 490), "Qualquer falha leva ao estado seguro.", font=f(R, 54), fill=ACC)
    d.text((300, 900), "Controle de AMV com FreeRTOS  ·  2026", font=f(R, 32), fill=MUT)

for n, fn in [("01_titulo", titulo), ("03_frenagem", frenagem), ("04_numeros", numeros), ("05_fim", fim)]:
    card(n, fn)

# legendas transparentes (lower third) para sobrepor na gravação
legs = {"leg_a_boot": "Boot: agulha vai para Normal, trava em 1,6 s → sinal verde",
        "leg_b_manobra": "Pedido de Reversa: sinal vermelho na hora · manobra de 3,1 s",
        "leg_c_trem": "Sinal vermelho à frente → o trem freia e para antes do sinal",
        "leg_d_espera": "Trem sobre o AMV: o pedido fica em espera",
        "leg_e_emerg": "Emergência: motor para e sinal vermelho em 200 µs",
        "leg_f_obstr": "Agulha obstruída: perda de detecção → vermelho em 253 µs"}
for n, t in legs.items():
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    ft = f(B, 44); w = d.textlength(t, font=ft)
    x0 = (W - w) / 2 - 40
    d.rounded_rectangle([x0, 900, x0 + w + 80, 990], 16, fill=(20, 22, 26, 215))
    d.rectangle([x0, 900, x0 + 10, 990], fill=ACC + (255,))
    d.text((x0 + 40, 918), t, font=ft, fill=FG + (255,))
    im.save(f"{OUT}/{n}.png")
