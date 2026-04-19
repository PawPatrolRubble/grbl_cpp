import os, glob

font_dir = "C:/Windows/Fonts"
# Check specific Chinese fonts
names = [
    "simhei.ttf", "simsun.ttc", "msyh.ttc", "simkai.ttf", "simfang.ttf",
    "STXINGKA.TTF", "STKAITI.TTF", "STFANGSO.TTF", "STSONG.TTF", "STZHONGS.TTF",
    "STLITI.TTF", "STCAIYUN.TTF", "STHUPO.TTF", "msyhbd.ttc", "SIMYOU.TTF",
    "FZSTK.TTF", "FZYTK.TTF",
]
found = []
for n in names:
    p = os.path.join(font_dir, n)
    if os.path.exists(p):
        found.append((n, p))
        print(f"  FOUND: {n}")
    else:
        # Try case insensitive
        for f in glob.glob(os.path.join(font_dir, n.lower())):
            found.append((os.path.basename(f), f))
            print(f"  FOUND: {os.path.basename(f)}")
            break

print(f"\nTotal found: {len(found)}")
