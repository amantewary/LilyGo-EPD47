#!/bin/bash
# Script to remove WiFi credentials from git history
# WARNING: This rewrites git history. Use with caution!

set -e

echo "⚠️  WARNING: This will rewrite git history!"
echo "If you've already pushed to GitHub, you'll need to force push after this."
echo ""
read -p "Continue? (yes/no): " confirm

if [ "$confirm" != "yes" ]; then
    echo "Aborted."
    exit 1
fi

# Remove credentials from all commits in history
git filter-branch --force --index-filter \
  'git rm --cached --ignore-unmatch examples/wifi_sync/wifi_sync.ino && \
   git checkout HEAD -- examples/wifi_sync/wifi_sync.ino && \
   sed -i.bak "s/Artemis_2.4GEXT/YOUR_WIFI_SSID/g; s/19E6942496D6/YOUR_WIFI_PASSWORD/g" examples/wifi_sync/wifi_sync.ino && \
   rm -f examples/wifi_sync/wifi_sync.ino.bak && \
   git add examples/wifi_sync/wifi_sync.ino' \
  --prune-empty --tag-name-filter cat -- --all

# Clean up backup refs
git for-each-ref --format="%(refname)" refs/original/ | xargs -n 1 git update-ref -d

# Force garbage collection
git reflog expire --expire=now --all
git gc --prune=now --aggressive

echo ""
echo "✅ Credentials removed from git history!"
echo ""
echo "Next steps:"
echo "1. Review the changes: git log --all"
echo "2. If satisfied, force push to GitHub:"
echo "   git push origin --force --all"
echo "   git push origin --force --tags"
echo ""
echo "⚠️  WARNING: Force pushing rewrites remote history!"
echo "   Only do this if you're sure no one else is using this branch."

