#!/usr/bin/env bash
# The product boundary (CLAUDE.md §2.1, D-038): a grader must confirm in sixty seconds that
# the only network code is the box's own page server. This is that confirmation.
#
#   check.sh ROOT                 check ROOT/src, ROOT/include and ROOT/Makefile
#   check.sh --selftest CASES     every CASES/bad_* must fail and every CASES/good_* pass
#
# The rules, applied to *.c and *.h, comments included:
#   1. No network family, resolver or address call outside src/interface/.
#   2. No HTTP or TLS library, no Bluetooth socket header, and no shelling out, anywhere.
#   3. socket() only in src/ipc/uds.c (Unix sockets, D-042, D-043) and src/interface/.
#   4. The Makefile links no network library.
set -uo pipefail

check() {
    local root=$1 bad=0 hits
    local dirs=()
    for d in src include; do [[ -d "$root/$d" ]] && dirs+=("$root/$d"); done

    scan() {  # scan PATTERN -> "file:line:text" for every C file under src/ and include/
        [[ ${#dirs[@]} -eq 0 ]] && return 0
        grep -rnE --include='*.c' --include='*.h' "$1" "${dirs[@]}" 2>/dev/null
    }
    report() {  # report RULE HITS
        [[ -z "$2" ]] && return 0
        printf '%s\n' "$2" | sed "s|^$root/||; s|^|  $1: |"
        bad=1
    }

    hits=$(scan '\b(AF|PF)_(INET6?|PACKET|NETLINK)\b|\b(getaddrinfo|gethostbyname|gethostbyaddr|getnameinfo|inet_(pton|ntop|addr|aton|ntoa))\b|#[[:space:]]*include[[:space:]]*<(netdb\.h|netinet/|arpa/inet\.h)' \
           | grep -v "^$root/src/interface/")
    report "network outside src/interface/" "$hits"

    hits=$(scan '#[[:space:]]*include[[:space:]]*<(curl/|openssl/|bluetooth/)|\b(curl_[a-z]|SSL_[A-Za-z])|\bAF_BLUETOOTH\b|\b(popen|system)[[:space:]]*\(')
    report "HTTP, TLS, Bluetooth socket or shell" "$hits"

    hits=$(scan '\bsocket(pair)?[[:space:]]*\(' \
           | grep -v -e "^$root/src/interface/" -e "^$root/src/ipc/uds\.c:")
    report "socket() outside src/ipc/uds.c and src/interface/" "$hits"

    if [[ -f "$root/Makefile" ]]; then
        hits=$(grep -nE '^[[:space:]]*(LDLIBS|LDFLAGS).*-l(curl|ssl|crypto|bluetooth)\b' "$root/Makefile" \
               | sed 's|^|Makefile:|')
        report "network library linked" "$hits"
    fi
    return $bad
}

if [[ "${1:-}" == "--selftest" ]]; then
    cases=${2:?usage: check.sh --selftest CASES}
    fail=0
    for c in "$cases"/bad_* "$cases"/good_*; do
        name=$(basename "$c")
        if check "$c" >/dev/null; then got=pass; else got=fail; fi
        case "$name:$got" in
            bad_*:fail|good_*:pass) ;;
            *) echo "boundary self-test: $name should not $got"; fail=1 ;;
        esac
    done
    exit $fail
fi

root=${1:?usage: check.sh ROOT | --selftest CASES}
if ! check "$root"; then
    echo "BOUNDARY VIOLATION: see CLAUDE.md §2.1 and DECISIONS D-038"
    exit 1
fi
