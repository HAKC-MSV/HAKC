#!/usr/bin/env bash

set -euo pipefail

BRANCH_TAG="${GIT_REF_NAME//\//-}"
BRANCH_LATEST_TAG="amd64-${BRANCH_TAG}-latest"

echo "Deterministic tag: $DET_TAG"
echo "Commit tag:        $COMMIT_TAG"
echo "Branch-latest tag: $BRANCH_LATEST_TAG"

envsubst < "$DOCKER_FILE_TEMPLATE" > Dockerfile

export _BUILDAH_STARTED_IN_USERNS=""
export BUILDAH_ISOLATION=chroot

buildah login -u "$jfrog_user" -p "$jfrog_pass" "$REPOURL"

IMAGE_BASE="$REPOURL/$REPOID/$IMAGE_ID"
TAGS=("$DET_TAG" "$COMMIT_TAG" "$BRANCH_LATEST_TAG")
if [[ -n "${ADD_LATEST_TAG:-}" ]]; then
  TAGS+=("amd64-latest")
fi

# Build once, tag multiple times
buildah bud --storage-driver vfs \
  $(printf -- '-t %s:%s ' "$IMAGE_BASE" "${TAGS[@]}") \
  .

# Push all tags (deterministic first because you want it as the cache key)
for tag in "${TAGS[@]}"; do
  buildah push --storage-driver vfs "$IMAGE_BASE:$tag"
done
