#!/usr/bin/env bash
set -Eeuo pipefail

IN_DIR="${1:-}"

if [[ -z "${IN_DIR}" ]]; then
  echo "Uso: $0 <input_dir>" >&2
  exit 2
fi

VIDEO_REGEX='\.((mp4)|(mkv)|(mov)|(avi)|(webm)|(m4v))$'

episode=1

shopt -s nullglob globstar

for pathName in "${IN_DIR}"/**/*; do
  if [[ -f "$pathName" ]]; then
    filename_lower="$(basename "$pathName" | tr '[:upper:]' '[:lower:]')"
    if [[ "$filename_lower" =~ $VIDEO_REGEX ]]; then
      filename=$(basename "$pathName" .mp4)
      #echo "$episode - $pathName - $filename"
      ./ingestionThroughChunk.sh 1 XXXXXXXXX "$filename" "\"RCS\", \"Motori\", \"SERIES\"" Motori_2_$episode Marco 2y "$pathName"
      ((episode++))
    fi
  fi
done


