# CoreELEC build

`scripts/build-coreelec.sh` builds this checkout as a CoreELEC 21 binary
addon for `Amlogic-ng.arm`. It uses the same CoreELEC build system and target
toolchain as a full CoreELEC image build.

The script:

1. checks out the pinned CoreELEC 21 revision;
2. applies `patches/coreelec-21-modern-host.patch`;
3. installs `package.mk` into CoreELEC's binary-addon packages;
4. exposes this repository to CoreELEC through a local `file://` source; and
5. copies the resulting installable ZIP to `dist/coreelec/`.

The compatibility patch contains the host compiler and download URL fixes
needed to build CoreELEC 21 on newer Linux distributions. It is intentionally
kept separate from the addon package definition so it can be reviewed or
removed independently.

## Local build

Install the host packages reported by CoreELEC's `scripts/checkdeps`, then run:

```bash
./scripts/build-coreelec.sh
```

The build is large and may take several hours the first time. Subsequent runs
reuse the CoreELEC checkout and toolchain under `.coreelec-build/`.

The defaults can be overridden with environment variables:

```bash
COREELEC_DIR=/path/to/CoreELEC-21.3 \
OUTPUT_DIR="$PWD/dist/coreelec" \
PROJECT=Amlogic-ce \
DEVICE=Amlogic-ng \
ARCH=arm \
./scripts/build-coreelec.sh
```

If `COREELEC_DIR` points to an existing checkout, the script preserves that
checkout and applies only the compatibility patch and addon package definition.

## GitHub Actions

The workflow in `.github/workflows/coreelec.yml` runs for relevant pull
requests, pushes to `personal`, releases, or a manual dispatch. It caches source
downloads and compiler output, then uploads the installable ZIP as a workflow
artifact.

To run it manually after the workflow is on the repository's default branch:

1. open the repository's **Actions** tab;
2. select **Build CoreELEC addon**;
3. choose **Run workflow** and the branch to build; and
4. download `pvr.dispatcharr-coreelec-amlogic-ng-arm` from the run's
   **Artifacts** section.

No repository secret or personal access token is required. The workflow only
needs read access to repository contents.
