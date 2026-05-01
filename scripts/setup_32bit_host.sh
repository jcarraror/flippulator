#!/usr/bin/env bash

set -euo pipefail

readonly REQUIRED_PACKAGES=(
  gcc-12-multilib
  libc6-dev-i386
  libsdl2-dev:i386
  libsdl2-ttf-dev:i386
  libbsd-dev:i386
)

install_requested=false
if [[ "${1:-}" == "--install" ]]; then
  install_requested=true
fi

have_i386_architecture() {
  dpkg --print-foreign-architectures | grep -qx 'i386'
}

package_installed() {
  dpkg-query -W -f='${Status}\n' "$1" 2>/dev/null | grep -qx 'install ok installed'
}

print_missing_packages() {
  local package
  for package in "${REQUIRED_PACKAGES[@]}"; do
    if ! package_installed "$package"; then
      printf '%s\n' "$package"
    fi
  done
}

main() {
  local -a missing_packages=()
  mapfile -t missing_packages < <(print_missing_packages)

  printf 'Checking flippulator 32-bit host requirements...\n'

  if have_i386_architecture; then
    printf '  [ok] i386 foreign architecture is enabled\n'
  else
    printf '  [missing] i386 foreign architecture is not enabled\n'
  fi

  if ((${#missing_packages[@]} == 0)); then
    printf '  [ok] required packages are installed\n'
  else
    printf '  [missing] required packages:\n'
    printf '    %s\n' "${missing_packages[@]}"
  fi

  if have_i386_architecture && ((${#missing_packages[@]} == 0)); then
    printf '\nHost looks ready for the 32-bit build.\n'
    exit 0
  fi

  printf '\nSuggested setup commands:\n'
  if ! have_i386_architecture; then
    printf '  sudo dpkg --add-architecture i386\n'
  fi
  printf '  sudo apt-get update\n'
  if ((${#missing_packages[@]} > 0)); then
    printf '  sudo apt-get install -y'
    printf ' %s' "${missing_packages[@]}"
    printf '\n'
  fi

  if [[ "$install_requested" == true ]]; then
    if ! have_i386_architecture; then
      sudo dpkg --add-architecture i386
    fi
    sudo apt-get update
    if ((${#missing_packages[@]} > 0)); then
      sudo apt-get install -y "${missing_packages[@]}"
    fi
  fi
}

main "$@"
