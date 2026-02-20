#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="$SCRIPT_DIR/.venv"

if [ -n "${VIRTUAL_ENV:-}" ]; then
  :
elif [ -d "$VENV_DIR" ]; then
  source "$VENV_DIR/bin/activate"
else
  echo "No .venv found" >&2
  exit 1
fi

PYTHON="${VIRTUAL_ENV}/bin/python"

"$PYTHON" -m pip --version &>/dev/null || "$PYTHON" -m ensurepip --upgrade

OS="$(uname -s)"
if [ "$OS" = "Darwin" ]; then
  xcode-select -p &>/dev/null || {
    echo "xcode cli required" >&2
    exit 1
  }
elif [ "$OS" = "Linux" ]; then
  command -v g++ &>/dev/null || sudo apt-get install -y build-essential python3-dev &>/dev/null
fi

command -v cmake &>/dev/null || "$PYTHON" -m pip install --quiet cmake
command -v ninja &>/dev/null || "$PYTHON" -m pip install --quiet ninja

"$PYTHON" -m pip install --quiet --upgrade pip
"$PYTHON" -m pip install --quiet ./potato_solver
"$PYTHON" -m pip install --quiet "numpy>=2.4.2,<3.0.0" "pygame>=2.6.1,<3.0.0"

echo "Ok."
