SO=extracted/lib/armeabi-v7a/libPirates.so
echo "=== codec intro.mp4"
python3 tools/mp4probe.py logs/intro.mp4 2>&1
echo "=== export Java_ di libPirates.so"
arm-none-eabi-nm -D $SO | grep ' T Java_' | sed 's/.* //'
echo "=== string intro/mp4/mediaplayer (tanpa simbol C++)"
strings -a $SO | grep -iE 'intro|movie|\.mp4|mediaplayer|playvideo|\.ogv|\.3gp' | grep -v '^_Z' | sort -u | head -30
echo "=== file dex di _apk"
ls -la _apk/*.dex 2>&1 | head
echo "=== string video di dex"
for f in _apk/*.dex; do echo "$f"; strings -a "$f" | grep -iE 'intro|MediaPlayer|VideoView|PlayMovie|PlayVideo|\.mp4' | sort -u | head -30; done
