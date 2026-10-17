#!/usr/bin/env python3
"""
Mock renderer for the Hiking Gadget OLED screens.

All text uses a hand-built 5x7 *bitmap* pixel font (defined below) — every glyph
is drawn on the pixel grid, so heights are uniform and there are no holes or
stray strokes. Small UI text renders at 1x, big labels at 2x. The big hero
digits (clock/timer/stopwatch/bearing) use a clean proportional digit font.
Coordinates mirror the firmware; the buffer is upscaled nearest-neighbour for an
authentic OLED pixel look. One PNG per screen + a combined contact sheet.

    python tools/render_screens.py
"""
from PIL import Image, ImageDraw, ImageFont
import os, math

W, H, SCALE = 128, 64, 6
HERE = os.path.dirname(__file__)
OUTDIR = os.path.join(HERE, "..", "screens")
SHEET  = os.path.join(HERE, "..", "screens_preview.png")

def font(size, bold=False):
    names = ["consolab.ttf"] if bold else ["consola.ttf", "consolab.ttf"]
    for n in names:
        try: return ImageFont.truetype("C:/Windows/Fonts/" + n, size)
        except: pass
    for n in ["DejaVuSansMono-Bold.ttf", "DejaVuSansMono.ttf"]:
        try: return ImageFont.truetype(n, size)
        except: pass
    return ImageFont.load_default()

FBIG  = font(18, True)        # hero digits  (logisoso16_tn)
FHUGE = font(26, True)        # hero digits  (logisoso24_tn)

# ── 5x7 bitmap font ───────────────────────────────────────────────────────────
# Each glyph: 7 rows of 5 columns. '#' = lit pixel. Baseline is the bottom row.
_F = {
'0':["  ## ","#   #","#  ##","# # #","##  #","#   #"," ### "],   # noqa
'1':["  #  "," ##  ","  #  ","  #  ","  #  ","  #  "," ### "],
'2':[" ### ","#   #","    #","   # ","  #  "," #   ","#####"],
'3':[" ### ","#   #","    #","  ## ","    #","#   #"," ### "],
'4':["   # ","  ## "," # # ","#  # ","#####","   # ","   # "],
'5':["#####","#    ","#### ","    #","    #","#   #"," ### "],
'6':["  ## "," #   ","#    ","#### ","#   #","#   #"," ### "],
'7':["#####","    #","   # ","  #  "," #   "," #   "," #   "],
'8':[" ### ","#   #","#   #"," ### ","#   #","#   #"," ### "],
'9':[" ### ","#   #","#   #"," ####","    #","   # "," ##  "],
'A':[" ### ","#   #","#   #","#####","#   #","#   #","#   #"],
'B':["#### ","#   #","#### ","#   #","#   #","#   #","#### "],
'C':[" ### ","#   #","#    ","#    ","#    ","#   #"," ### "],
'D':["###  ","#  # ","#   #","#   #","#   #","#  # ","###  "],
'E':["#####","#    ","#    ","#### ","#    ","#    ","#####"],
'F':["#####","#    ","#    ","#### ","#    ","#    ","#    "],
'G':[" ### ","#   #","#    ","# ###","#   #","#   #"," ### "],
'H':["#   #","#   #","#   #","#####","#   #","#   #","#   #"],
'I':[" ### ","  #  ","  #  ","  #  ","  #  ","  #  "," ### "],
'J':["  ###","   # ","   # ","   # ","   # ","#  # "," ##  "],
'K':["#   #","#  # ","# #  ","##   ","# #  ","#  # ","#   #"],
'L':["#    ","#    ","#    ","#    ","#    ","#    ","#####"],
'M':["#   #","## ##","# # #","#   #","#   #","#   #","#   #"],
'N':["#   #","##  #","# # #","#  ##","#   #","#   #","#   #"],
'O':[" ### ","#   #","#   #","#   #","#   #","#   #"," ### "],
'P':["#### ","#   #","#   #","#### ","#    ","#    ","#    "],
'Q':[" ### ","#   #","#   #","#   #","# # #","#  # "," ## #"],
'R':["#### ","#   #","#   #","#### ","# #  ","#  # ","#   #"],
'S':[" ####","#    ","#    "," ### ","    #","    #","#### "],
'T':["#####","  #  ","  #  ","  #  ","  #  ","  #  ","  #  "],
'U':["#   #","#   #","#   #","#   #","#   #","#   #"," ### "],
'V':["#   #","#   #","#   #","#   #","#   #"," # # ","  #  "],
'W':["#   #","#   #","#   #","# # #","# # #","## ##","#   #"],
'X':["#   #","#   #"," # # ","  #  "," # # ","#   #","#   #"],
'Y':["#   #","#   #"," # # ","  #  ","  #  ","  #  ","  #  "],
'Z':["#####","    #","   # ","  #  "," #   ","#    ","#####"],
'a':["     ","     "," ### ","    #"," ####","#   #"," ####"],
'b':["#    ","#    ","#### ","#   #","#   #","#   #","#### "],
'c':["     ","     "," ### ","#    ","#    ","#   #"," ### "],
'd':["    #","    #"," ####","#   #","#   #","#   #"," ####"],
'e':["     ","     "," ### ","#   #","#####","#    "," ### "],
'f':["  ## "," #  #"," #   ","#### "," #   "," #   "," #   "],
'g':["     ","     "," ####","#   #"," ####","    #"," ### "],
'h':["#    ","#    ","#### ","#   #","#   #","#   #","#   #"],
'i':["  #  ","     "," ##  ","  #  ","  #  ","  #  "," ### "],
'j':["   # ","     ","  ## ","   # ","   # ","#  # "," ##  "],
'k':["#    ","#    ","#  # ","# #  ","##   ","# #  ","#  # "],
'l':[" ##  ","  #  ","  #  ","  #  ","  #  ","  #  "," ### "],
'm':["     ","     ","## # ","# # #","# # #","#   #","#   #"],
'n':["     ","     ","#### ","#   #","#   #","#   #","#   #"],
'o':["     ","     "," ### ","#   #","#   #","#   #"," ### "],
'p':["     ","     ","#### ","#   #","#### ","#    ","#    "],
'q':["     ","     "," ####","#   #"," ####","    #","    #"],
'r':["     ","     ","# ## ","##   ","#    ","#    ","#    "],
's':["     ","     "," ####","#    "," ### ","    #","#### "],
't':[" #   "," #   ","#### "," #   "," #   "," #  #","  ## "],
'u':["     ","     ","#   #","#   #","#   #","#   #"," ####"],
'v':["     ","     ","#   #","#   #","#   #"," # # ","  #  "],
'w':["     ","     ","#   #","#   #","# # #","# # #"," # # "],
'x':["     ","     ","#   #"," # # ","  #  "," # # ","#   #"],
'y':["     ","     ","#   #","#   #"," ####","    #"," ### "],
'z':["     ","     ","#####","   # ","  #  "," #   ","#####"],
' ':["     ","     ","     ","     ","     ","     ","     "],
':':["     ","  #  ","  #  ","     ","  #  ","  #  ","     "],
'.':["     ","     ","     ","     ","     "," ##  "," ##  "],
',':["     ","     ","     ","     "," ##  "," ##  "," #   "],
'%':["##  #","##  #","   # ","  #  "," #   ","#  ##","#  ##"],
'/':["    #","    #","   # ","  #  "," #   ","#    ","#    "],
'-':["     ","     ","     ","#####","     ","     ","     "],
'+':["     ","  #  ","  #  ","#####","  #  ","  #  ","     "],
'(':["   # ","  #  "," #   "," #   "," #   ","  #  ","   # "],
')':[" #   ","  #  ","   # ","   # ","   # ","  #  "," #   "],
'>':["#    "," #   ","  #  ","   # ","  #  "," #   ","#    "],
'<':["    #","   # ","  #  "," #   ","  #  ","   # ","    #"],
'?':[" ### ","#   #","    #","  ## ","  #  ","     ","  #  "],
'!':["  #  ","  #  ","  #  ","  #  ","  #  ","     ","  #  "],
}
def _glyph(ch): return _F.get(ch, _F['?'])


def overlay(o, active, battery=80):
    N, DOT, GAP = 5, 3, 3
    total = (N-1)*(DOT+GAP)+DOT; sx = (W-total)//2
    for i in range(N):
        x = sx + i*(DOT+GAP)
        if i == active: o.fill(x, 2, DOT, 3)
        else: o.px(x+1, 3)
    if battery is not None:
        x, y = W-15, 0
        o.rect(x, y, 12, 6); o.fill(x+12, y+2, 2, 2)
        fw = (battery*10+50)//100
        if fw: o.fill(x+1, y+1, fw, 4)


class OLED:
    def __init__(self):
        self.img = Image.new("1", (W, H), 0)
        self.d = ImageDraw.Draw(self.img)
    def hline(self, x, y, w):  self.d.line([(x, y), (x + w - 1, y)], fill=1)
    def vline(self, x, y, h):  self.d.line([(x, y), (x, y + h - 1)], fill=1)
    def rect(self, x, y, w, h):self.d.rectangle([x, y, x+w-1, y+h-1], outline=1)
    def fill(self, x, y, w, h, c=1): self.d.rectangle([x, y, x+w-1, y+h-1], fill=c)
    def line(self, a, b, c, e): self.d.line([(a, b), (c, e)], fill=1)
    def tri(self, pts):  self.d.polygon(pts, fill=1)
    def circle(self, cx, cy, r):  self.d.ellipse([cx-r, cy-r, cx+r, cy+r], outline=1)
    def fcircle(self, cx, cy, r): self.d.ellipse([cx-r, cy-r, cx+r, cy+r], fill=1)
    def px(self, x, y):
        if 0 <= x < W and 0 <= y < H: self.img.putpixel((x, y), 1)

    # ── Bitmap text (scale = integer pixel size) ─────────────────────────────
    def bmp(self, x, baseline, s, sc=1, c=1):
        adv = 5*sc + 1             # 5px glyph + 1px gap (gap unscaled)
        top = baseline - (7*sc - 1)
        for i, ch in enumerate(s):
            cx = x + i*adv
            for r, row in enumerate(_glyph(ch)):
                for col, p in enumerate(row):
                    if p == '#':
                        gx, gy = cx + col*sc, top + r*sc
                        self.d.rectangle([gx, gy, gx+sc-1, gy+sc-1], fill=c)
    def bw(self, s, sc=1):  return len(s) * (5*sc + 1)
    def cbmp(self, baseline, s, sc=1, c=1):
        self.bmp((W - self.bw(s, sc)) // 2, baseline, s, sc, c)

    # ── Proportional hero digits ─────────────────────────────────────────────
    def text(self, x, baseline, s, f, c=1):
        self.d.text((x, baseline - f.getmetrics()[0]), s, font=f, fill=c)
    def tw(self, s, f): return int(self.d.textlength(s, font=f))
    def ctext(self, baseline, s, f, c=1): self.text((W-self.tw(s,f))//2, baseline, s, f, c)

    # ── Icons (mirror UI::icon*) ─────────────────────────────────────────────
    def thermo(self, x, y):
        self.px(x+2, y); self.vline(x+1, y+1, 5); self.vline(x+3, y+1, 5)
        self.fill(x+2, y+3, 1, 5); self.fcircle(x+2, y+8, 2)
    def drop(self, x, y):
        self.tri([(x+2, y), (x, y+4), (x+4, y+4)]); self.fcircle(x+2, y+5, 2)


def header(o, t): o.bmp(0, 7, t); o.hline(0, 9, W)
def footer(o, k1, k2=None):
    o.hline(0, 54, W)
    if k1: o.bmp(2, 63, k1)
    if k2: o.bmp(66, 63, k2)

# ── Screens ───────────────────────────────────────────────────────────────────
def s_splash():
    o = OLED()
    for seg in [(6,24,26,4),(26,4,44,24),(34,24,64,2),(64,2,92,24),(80,24,102,8),(102,8,122,24)]:
        o.line(*seg)
    o.hline(2,24,124); o.line(60,8,64,4); o.line(64,4,68,8)
    o.line(23,9,26,6); o.line(26,6,29,9)
    o.cbmp(40,"HIKE GADGET",2); o.cbmp(50,"v1.0  ESP32-S3",1)
    o.rect(2,56,124,6); o.fill(3,57,80,4)
    return o, "01_splash"

def s_home():
    o = OLED(); o.bmp(0,7,"MSK DP14"); o.hline(0,9,W); overlay(o,0,80)
    o.ctext(30,"12:34:56",FBIG); o.cbmp(45,"Sat  21.06.2026",1)
    o.thermo(1,52); o.bmp(8,63,"23.4",1)
    nx=8+o.bw("23.4",1); o.circle(nx+2,58,1); o.bmp(nx+5,63,"C",1)
    hum="56%"; hw=o.bw(hum,1); o.bmp(W-hw-2,63,hum,1); o.drop(W-hw-9,55)
    return o, "02_home"

def s_stopwatch():
    o = OLED(); header(o,"STOPWATCH"); overlay(o,1)
    o.ctext(27,"01:23.45",FBIG); o.cbmp(37,"RUNNING",1)
    o.bmp(4,45,"Lap 2",1); t="01:10.33"; o.bmp(W-o.bw(t,1)-4,45,t,1)
    o.bmp(4,53,"Lap 1",1); t="00:45.12"; o.bmp(W-o.bw(t,1)-4,53,t,1)
    footer(o,"K1:STOP","K2:LAP")
    return o, "03_stopwatch"

def s_timer_select():
    o = OLED(); header(o,"TIMER"); overlay(o,2)
    ps=["1min","3min","5min","10min","30min","60min"]; CX=[6,50,92]; RY=[32,48]; sel=2
    for i,p in enumerate(ps):
        bx,by=CX[i%3],RY[i//3]
        if i==sel:
            tw=o.bw(p,1); o.fill(bx-3,by-7,tw+5,11,1); o.bmp(bx,by,p,1,0)
        else: o.bmp(bx,by,p,1)
    footer(o,"K1:NEXT","K2:START")
    return o, "04_timer_select"

def s_timer_run():
    o = OLED(); header(o,"TIMER"); overlay(o,2)
    o.ctext(40,"04:37",FHUGE); o.rect(2,45,124,6); o.fill(2,45,40,6)
    footer(o,"K2:STOP")
    return o, "05_timer_running"

def _card(o, s, px, py):           # centre a single cardinal glyph on (px,py)
    o.bmp(px - 2, py + 3, s, 1)

def compass_rose(o, needle_deg, target=None):
    CX,CY,CR=90,31,26
    o.circle(CX,CY,CR)
    for i in range(12):            # graduated ticks; cardinals longer
        a=i*math.pi/6; inner=(CR-4) if i%3==0 else (CR-2)
        o.line(CX+int(inner*math.sin(a)),CY-int(inner*math.cos(a)),
               CX+int((CR-1)*math.sin(a)),CY-int((CR-1)*math.cos(a)))
    LR=CR-12
    _card(o,"N",CX,CY-LR); _card(o,"S",CX,CY+LR)
    _card(o,"E",CX+LR,CY); _card(o,"W",CX-LR,CY)
    if target is not None:
        a=math.radians(target); mx=CX+int((CR+1)*math.sin(a)); my=CY-int((CR+1)*math.cos(a))
        o.fcircle(mx,my,2); o.circle(mx,my,3)
    a=math.radians(needle_deg); s,c=math.sin(a),math.cos(a)
    ps,pc=math.sin(a+math.pi/2),math.cos(a+math.pi/2)
    Rn,Rs,bw=CR-8,CR-14,2
    tip=(CX+int(Rn*s),CY-int(Rn*c))
    bA=(CX+int(bw*ps),CY-int(bw*pc)); bB=(CX-int(bw*ps),CY+int(bw*pc))
    o.tri([tip,bA,bB])                                # north arrowhead (filled)
    o.line(CX,CY,CX-int(Rs*s),CY+int(Rs*c))           # south tail
    o.fcircle(CX,CY,1)

def s_compass():
    o = OLED(); overlay(o,3,80)
    compass_rose(o,45,target=120)
    o.text(2,22,"045",FBIG); nx=2+o.tw("045",FBIG); o.circle(nx+3,9,2)
    o.bmp(2,41,"NE",2)
    o.bmp(2,55,"tgt 120",1); o.bmp(2,63,"K2 clear",1)
    return o, "06_compass"

def _settings(rows, sel, scrolltop, name):
    o = OLED(); header(o,"SETTINGS")
    Y0,STEP,VIS,COUNT=20,10,4,9
    for r,(lab,val) in enumerate(rows):
        y=Y0+r*STEP
        if r==sel:
            o.fill(0,y-8,W-4,STEP,1); o.bmp(4,y,lab,1,0)
            o.bmp(W-o.bw(val,1)-6,y,val,1,0)
        else:
            o.bmp(4,y,lab,1); o.bmp(W-o.bw(val,1)-6,y,val,1)
    trackY,trackH=11,43
    o.vline(W-1,trackY,trackH)
    thumbH=max(4,trackH*VIS//COUNT)
    thumbY=trackY+(trackH-thumbH)*scrolltop//(COUNT-VIS)
    o.fill(W-3,thumbY,3,thumbH,1)
    footer(o,"K1:next","K2:change")
    return o, name

def s_settings_list():
    return _settings([("Timezone","MSK"),("Timeout","20s"),
                      ("Heater","OFF"),("Low power","ON")], 0, 0, "07_settings_top")

def s_settings_scroll():
    return _settings([("Calibrate compass",">"),("Sync time (WiFi)",">"),
                      ("WiFi setup",">"),("Power off",">")], 3, 5, "08_settings_scroll")

def s_calib():
    o = OLED(); header(o,"CALIBRATE")
    o.cbmp(24,"Rotate slowly",1); o.cbmp(35,"in a figure-8",1)
    o.rect(2,39,124,6); o.fill(2,39,86,6); o.cbmp(53,"70%",1)
    footer(o,"K1:cancel","K2:save")
    return o, "09_compass_calib"

def s_wifi():
    o = OLED(); header(o,"WIFI")
    o.bmp(4,26,"AP: HikeGadget-Setup",1)
    o.bmp(4,40,"pass: hike1234",1)
    o.bmp(4,54,"go 192.168.4.1",1)
    return o, "10_wifi_portal"

def s_poweroff():
    o = OLED()
    o.cbmp(30,"Power off",2); o.cbmp(46,"press any key to wake",1)
    return o, "11_power_off"

SCREENS = [s_splash,s_home,s_stopwatch,s_timer_select,s_timer_run,s_compass,
           s_settings_list,s_settings_scroll,s_calib,s_wifi,s_poweroff]

def main():
    os.makedirs(OUTDIR, exist_ok=True)
    tiles=[]; label_h=16
    for fn in SCREENS:
        o,name = fn()
        big = o.img.convert("L").resize((W*SCALE, H*SCALE), Image.NEAREST)
        big.save(os.path.join(OUTDIR, name + ".png"))
        tile = Image.new("L",(W*SCALE+4, H*SCALE+label_h+4),40)
        tile.paste(big,(2,label_h))
        d=ImageDraw.Draw(tile); d.rectangle([1,label_h-1,W*SCALE+2,H*SCALE+label_h],outline=120)
        d.text((4,2),name,font=font(12),fill=255)
        tiles.append(tile)
    cols=2; tw,th=tiles[0].width,tiles[0].height; rows=(len(tiles)+cols-1)//cols
    sheet=Image.new("L",(cols*tw+(cols+1)*10, rows*th+(rows+1)*10),20)
    for i,t in enumerate(tiles):
        r,c=divmod(i,cols); sheet.paste(t,(10+c*(tw+10),10+r*(th+10)))
    sheet.save(SHEET)
    print("wrote", len(SCREENS), "screens to", os.path.abspath(OUTDIR))
    print("sheet:", os.path.abspath(SHEET), sheet.size)

if __name__ == "__main__":
    main()
