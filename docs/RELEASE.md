# Releases

Packages are published in [GitHub Releases](https://github.com/troveminecraft/ShanHai/releases).
The repository contains source code, build scripts and required resources.

The product version and Minecraft compatibility target are independent. A product
release may include multiple packages, each labelled with its platform and exact
Minecraft build. Use one release per product version in each repository. Windows
client packages are public; BDS packages remain in the private upstream repository.
Android is no longer maintained.

## Publishing

1. Update manifests, version constants and the changelog. Pin the game build,
   loader and runtime-data versions for each package.
2. Build and check each package separately. Record source commits and package
   hashes; retain previous versions for rollback.
3. Use [the release template](RELEASE_TEMPLATE.md). Describe changes, compatibility,
   installation, save migration and concrete known issues in product language.
4. Run `python tools/check_release_text.py README.md README.zh-CN.md CHANGELOG.md`
   and run the same checker on the release notes before uploading.
5. Upload the matching packages and checksums. Set the release channel explicitly;
   documentation edits do not change its prerelease status.

Historical attachment documentation can be corrected while preserving every
executable and world-data byte. Keep the original archive and record both hashes;
update published checksums after replacing the archive. Source commits and tags
identify immutable code snapshots and must not be silently retargeted.

## Licensing

Original project code uses the [MIT License](../LICENSE). Keep third-party notices
and licenses in every package; see [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).
Minecraft binaries and vanilla structure templates are not part of the mod package.
