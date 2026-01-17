#!/bin/bash
# scripts/update-version.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

usage() {
    echo "Usage: $0 [OPTIONS] <new_version> [debian_revision]"
    echo ""
    echo "Options:"
    echo "  --since TAG    Compare with specific tag"
    echo "  -h, --help     Show this help"
    echo ""
    echo "Examples:"
    echo "  $0 1.5.0              # Release"
    echo "  $0 1.5.0 2            # Second Debian packaging"
    echo "  $0 1.5.0-rc1          # Pre-release"
    echo "  $0 --since v1.4.0 1.5.0   # Explicit base version"
    echo ""
    exit 1
}

NEW_VERSION=""
DEBIAN_REV="1"
SINCE_TAG=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --since)
            SINCE_TAG="$2"
            shift 2
            ;;
        --help|-h)
            usage
            ;;
        *)
            if [ -z "$NEW_VERSION" ]; then
                NEW_VERSION="$1"
            else
                DEBIAN_REV="$1"
            fi
            shift
            ;;
    esac
done

# Проверка что версия указана
if [ -z "$NEW_VERSION" ]; then
    echo "Error: Version is required"
    echo ""
    usage
fi

NEW_VERSION="${NEW_VERSION#v}"

echo "Updating version to: $NEW_VERSION"
echo "   Debian revision: $DEBIAN_REV"
echo ""

cd "$PROJECT_DIR"

find_previous_version() {
    local current_version="$1"

    if [ -n "$SINCE_TAG" ]; then
        echo "$SINCE_TAG"
        return
    fi

    local all_tags
    all_tags=$(git tag --sort=-version:refname)

    if [[ ! "$current_version" =~ (rc|alpha|beta) ]]; then
        for tag in $all_tags; do
            tag="${tag#v}"
            if [[ "$tag" != "$current_version"* ]] && \
               [[ ! "$tag" =~ (rc|alpha|beta) ]]; then
                echo "v$tag"
                return
            fi
        done
    else
        local base_version="${current_version%%-*}"  # 1.5.0-rc1 -> 1.5.0

        for tag in $all_tags; do
            tag="${tag#v}"
            if [[ "$tag" != "$base_version"* ]] && \
               [[ ! "$tag" =~ (rc|alpha|beta) ]]; then
                echo "v$tag"
                return
            fi
        done
    fi

    git describe --tags --abbrev=0 HEAD^ 2>/dev/null || echo ""
}

PREV_TAG=$(find_previous_version "$NEW_VERSION")

if [ -n "$PREV_TAG" ]; then
    echo "Comparing with: $PREV_TAG"
    COMMIT_COUNT=$(git rev-list --count "$PREV_TAG..HEAD")
    echo "   Commits since then: $COMMIT_COUNT"
    echo ""
else
    echo "No previous version found, showing recent commits"
    PREV_TAG="HEAD~20"
    echo ""
fi

echo "Change base version for changelog?"
echo "  Current: $PREV_TAG"
read -p "Enter different tag or press Enter to continue: " CUSTOM_TAG
if [ -n "$CUSTOM_TAG" ]; then
    PREV_TAG="$CUSTOM_TAG"
    echo "Using $PREV_TAG as base"
fi
echo ""

echo "$NEW_VERSION" > VERSION
echo "Updated VERSION file"

DEBIAN_VERSION="$NEW_VERSION"
if [[ "$NEW_VERSION" =~ (alpha|beta|rc) ]]; then
    DEBIAN_VERSION="${NEW_VERSION/-/~}"
fi
DEBIAN_FULL_VERSION="${DEBIAN_VERSION}-${DEBIAN_REV}"

TEMP_CHANGELOG=$(mktemp)

cat > "$TEMP_CHANGELOG" << EOF
odyssey ($DEBIAN_FULL_VERSION) unstable; urgency=medium

  * New upstream release $NEW_VERSION

  [ Changes since $PREV_TAG - edit/remove as needed ]
EOF

git log --pretty=format:"  * %s" "$PREV_TAG..HEAD" | while IFS= read -r line; do
    if [[ ! "$line" =~ "Merge pull request" ]] && \
       [[ ! "$line" =~ "Merge branch" ]] && \
       [[ ! "$line" =~ "Release version" ]] && \
       [[ ! "$line" =~ ^[[:space:]]*$ ]]; then
        echo "$line" >> "$TEMP_CHANGELOG"
    fi
done

cat >> "$TEMP_CHANGELOG" << EOF

 -- ${DEBFULLNAME:-Yandex Database Team} <${DEBEMAIL:-mdb-admin@yandex-team.ru}>  $(date -R)

EOF

if [ -f debian/changelog ]; then
    cat debian/changelog >> "$TEMP_CHANGELOG"
fi

mkdir -p debian
mv "$TEMP_CHANGELOG" debian/changelog

echo "Opening editor..."
echo ""
echo "Tip: Group changes into categories:"
echo "   - New features"
echo "   - Bug fixes"
echo "   - Performance improvements"
echo ""
read -p "Press Enter to open editor..."

${EDITOR:-vim} debian/changelog

echo ""
echo "Updated debian/changelog"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "New version: $NEW_VERSION"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
git diff --no-index /dev/null VERSION 2>/dev/null | tail -n +4 || cat VERSION
echo ""
echo "First changelog entry:"
head -n 15 debian/changelog
echo ""

git status --short VERSION debian/changelog

echo ""
read -p "Commit these changes? (y/N) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    git add VERSION debian/changelog
    git commit -m "Release version $NEW_VERSION"
    echo "Changes committed"

    TAG_NAME="v$NEW_VERSION"
    echo ""
    read -p "Create git tag $TAG_NAME? (y/N) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        TAG_MESSAGE=$(awk '/^odyssey \(/,/^ --/' debian/changelog | head -n -1)
        git tag -a "$TAG_NAME" -m "$TAG_MESSAGE"
        echo "Tag $TAG_NAME created"
        echo ""
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        echo "Release $NEW_VERSION is ready!"
        echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        echo ""
        echo "Push with:"
        echo "  git push origin main"
        echo "  git push origin $TAG_NAME"
    fi
fi
