#!/bin/sh

tag_name="v5.15"

env GIT_TAG=$tag_name $(git rev-parse --show-toplevel)/scripts/patch-generation/apply_patches_to_tag.sh

exit 0
