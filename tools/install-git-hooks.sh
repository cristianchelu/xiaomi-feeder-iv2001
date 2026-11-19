#!/usr/bin/env bash
# Install git hooks that re-sync SDK patches after checkout/merge.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(git -C "$SCRIPT_DIR/.." rev-parse --show-toplevel)"
HOOKS_DIR="$(git -C "$REPO_ROOT" rev-parse --path-format=absolute --git-path hooks)"

install_hook() {
    local name="$1"
    local hook_path="$HOOKS_DIR/$name"
    cat > "$hook_path" <<'EOF'
#!/usr/bin/env bash
# Auto-installed by tools/install-git-hooks.sh
set -euo pipefail
REPO_ROOT="$(git rev-parse --show-toplevel)"
if [ -x "$REPO_ROOT/tools/sync-sdk-patches.sh" ]; then
    "$REPO_ROOT/tools/sync-sdk-patches.sh" --if-needed || true
fi
EOF
    chmod +x "$hook_path"
    echo "Installed $hook_path"
}

mkdir -p "$HOOKS_DIR"
install_hook post-checkout
install_hook post-merge
echo "Git hooks ready. SDK patches re-sync after checkout/merge when the tree drifts."
