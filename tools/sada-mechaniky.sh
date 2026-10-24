#!/bin/bash
# Builds the drive sound set in src/res/zvuky-mechaniky from Luboš Krejča's
# recordings of a real Eureka A4 drive (HANDOFF 6.54, PUVOD.md there).  The
# recordings themselves are not in the tree; this is how the set was cut from
# them, so that it can be cut again.
#
# Usage: tools/sada-mechaniky.sh RECORDINGS_DIR OUTPUT_DIR [GAIN]
#
# The recordings are already WAV, PCM, 48 kHz, mono, 16-bit.  One gain for
# the whole set keeps them in their own proportions; 1.4 is as far as the
# loudest of them goes without clipping.
set -e
S="$1"
O="$2"
G="${3:-1.4}"
if [ -z "$S" ] || [ -z "$O" ]; then
  echo "Použitie: $0 PRIEČINOK_NAHRÁVOK CIEĽ [ZISK]" >&2
  exit 1
fi
mkdir -p "$O"
OUT=(-ac 1 -ar 48000 -c:a pcm_s16le -bitexact -fflags +bitexact -map_metadata -1 -y -loglevel error)
D="$S/Diskove funkce.wav"

# Spin-up: from the start of the recording to where the loop begins.  The
# motor's pitch (about 200 Hz) does not rise in it; the drive is up to speed
# almost at once.
ffmpeg -i "$D" -af "atrim=0:0.30,asetpts=PTS-STARTPTS,afade=t=in:d=0.02,volume=$G" \
  "${OUT[@]}" "$O/motor-rozbeh.wav"

# Loop: 0.30-1.05 s of the steady motor before the step, its last 0.1 s
# crossfaded with 0.20-0.30 s, so its end runs into its start and the
# spin-up, which ends at 0.30 s, runs straight into it.
ffmpeg -i "$D" -filter_complex "\
[0]asplit=3[a][b][c];\
[a]atrim=0.30:0.95,asetpts=PTS-STARTPTS[body];\
[b]atrim=0.95:1.05,asetpts=PTS-STARTPTS,afade=t=out:d=0.1[tail];\
[c]atrim=0.20:0.30,asetpts=PTS-STARTPTS,afade=t=in:d=0.1[head];\
[tail][head]amix=inputs=2:normalize=0[seam];\
[body][seam]concat=n=2:v=0:a=1,volume=$G" "${OUT[@]}" "$O/motor-slucka.wav"

# Run-down: from where the motor starts to fade to the end of the recording,
# which stops before the motor does -- hence the fade at the end.
ffmpeg -i "$D" -af "atrim=2.00:4.85,asetpts=PTS-STARTPTS,afade=t=out:st=2.25:d=0.6,volume=$G" \
  "${OUT[@]}" "$O/motor-dobeh.wav"

# The step: Pohyb7 (pohyb8 is the same take).  The step in Diskove funkce
# is two strikes 50 ms apart -- the first move after the spin-up, not an
# ordinary step -- and played in turn with this one it made every other step
# of a format sound doubled, so it is left out.
ffmpeg -i "$S/Pohyb7.wav" -af "afade=t=in:d=0.002,afade=t=out:st=0.06:d=0.038,volume=$G" \
  "${OUT[@]}" "$O/krok-1.wav"

# Rake: pohyb1, the one that clicks every 6 ms -- the ROM's step rate.  The
# others were made at other rates (12 ms in pohyb5 and pohyb6, 2-3 ms in
# pohyb2 to pohyb4), most likely by DiskEdit.
ffmpeg -i "$S/pohyb1.wav" -af "afade=t=in:d=0.005,afade=t=out:st=0.52:d=0.04,volume=$G" \
  "${OUT[@]}" "$O/presun.wav"
