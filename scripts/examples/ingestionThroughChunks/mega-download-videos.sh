#!/usr/bin/env bash
set -Eeuo pipefail

LINKS_FILE="${1:-}"
OUT_DIR="${2:-}"

if [[ -z "${LINKS_FILE}" || -z "${OUT_DIR}" ]]; then
  echo "Uso: $0 <links.txt> <output_dir>" >&2
  exit 2
fi

if [[ ! -f "${LINKS_FILE}" ]]; then
  echo "File non trovato: ${LINKS_FILE}" >&2
  exit 2
fi

command -v mega-get >/dev/null 2>&1 || {
  echo "Errore: mega-get non trovato. Installa MEGAcmd." >&2
  exit 2
}

mkdir -p "${OUT_DIR}"
LOG_FILE="${OUT_DIR}/download.log"

VIDEO_REGEX='\.((mp4)|(mkv)|(mov)|(avi)|(webm)|(m4v))$'

episode=1
i=0
while IFS= read -r raw || [[ -n "$raw" ]]; do
  url="$(echo "$raw" | sed -e 's/[[:space:]]*$//' -e 's/^[[:space:]]*//')"
  [[ -z "$url" ]] && continue
  [[ "$url" =~ ^# ]] && continue

  i=$((i+1))
  subdir="$(printf "%03d" "$i")"
  dest="${OUT_DIR}/${subdir}"
  mkdir -p "${dest}"

  echo "[$(date -Is)] #${subdir} START ${url}" | tee -a "${LOG_FILE}"

  set +e
  # Sintassi compatibile con molte versioni:
  # mega-get <exportedlink|remotepath> [localpath]
  mega-get "${url}" "${dest}" >>"${LOG_FILE}" 2>&1
  rc=$?
  set -e

  if [[ $rc -ne 0 ]]; then
    echo "[$(date -Is)] #${subdir} ERROR (exit=${rc}) ${url}" | tee -a "${LOG_FILE}"
    continue
  fi

  shopt -s globstar nullglob
  for f in "${dest}"/**/*; do
    if [[ -f "$f" ]]; then
      filename_lower="$(basename "$f" | tr '[:upper:]' '[:lower:]')"
      if [[ "$filename_lower" =~ $VIDEO_REGEX ]]; then
	#da modificare in base ai contenuti...
        echo "$episode" > ./episode.txt
        filename=$(basename "$f" .mp4)
        ./ingestionThroughChunk.sh 1 XXXXXXXXX "$filename" "\"RCS\", \"Motori\", \"SERIES\"" Motori_2_$episode Marco 2y "$f"
        ((episode++))
      fi
    fi
  done

  #find "${dest}" -type d -empty -delete 2>/dev/null || true

  echo "[$(date -Is)] #${subdir} DONE ${url}" | tee -a "${LOG_FILE}"
done < "${LINKS_FILE}"

echo "Finito. Log: ${LOG_FILE}"

