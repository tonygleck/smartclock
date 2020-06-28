#!/bin/bash

# Licensed under the MIT license. See LICENSE file in the project root for full license information.

set -e

script_dir=$(cd "$(dirname "$0")" && pwd)
root_dir=$(cd "${script_dir}/.." && pwd)

repo_array=($root_dir"/deps/lib-util-c" $root_dir"/deps/patchcords" $root_dir"/deps/http_client" $root_dir"/deps/parson")

# update lib-util-c
for repo_dir in ${repo_array[*]}
do
    pushd $repo_dir
    git checkout master
    git pull origin master
    popd
done

: