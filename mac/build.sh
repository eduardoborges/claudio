#!/bin/sh
# Builds the app in mac/build, plus mac/build/Claudio.dmg with --dmg. The version
# comes from version.txt.
# SIGN_IDENTITY signs for distribution (Developer ID, hardened runtime) and
# builds Claudio.app. NOTARY_KEY_ID, NOTARY_ISSUER_ID and NOTARY_KEY_P8 add
# notarization.
# Any other build is Claudio Dev.app, with its own bundle id, settings and
# Bluetooth grant, so it runs next to the installed Claudio. It signs with
# SIGN_ID or the first valid "Apple Development" certificate, because macOS
# ties the Bluetooth permission to the signature and asks again whenever it
# changes. With no identity it signs ad hoc.
set -e
: "${SIGN_ID:=$(security find-identity -v -p codesigning \
  | awk '/Apple Development/ && !/REVOKED/ { print $2; exit }')}"
cd "$(dirname "$0")"
swift build -c release --product Claudio
name=Claudio
[ -n "$SIGN_IDENTITY" ] || name="Claudio Dev"
app="build/$name.app"
rm -rf "$app"
mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources"
cp "$(swift build -c release --show-bin-path)/Claudio" "$app/Contents/MacOS/"
cp ../daemon/attention-hook.sh "$app/Contents/Resources/"
# AppIcon.icon is an Icon Composer document: Clawd typing on the laptop, a frame
# of the official animation, on the display's black. actool turns it into
# Assets.car, which macOS 26 needs to draw the icon without a gray frame, plus an
# .icns for older systems.
xcrun actool AppIcon.icon --compile "$app/Contents/Resources" --platform macosx \
  --minimum-deployment-target 14.0 --app-icon AppIcon \
  --output-partial-info-plist "$(mktemp -d)/icon.plist" > /dev/null
cp Info.plist "$app/Contents/"
version=$(cat ../version.txt)
plutil -replace CFBundleShortVersionString -string "$version" "$app/Contents/Info.plist"
plutil -replace CFBundleVersion -string "$version" "$app/Contents/Info.plist"
if [ -z "$SIGN_IDENTITY" ]; then
  plutil -replace CFBundleIdentifier -string dev.eduardoborges.claudio.dev "$app/Contents/Info.plist"
  plutil -replace CFBundleName -string "$name" "$app/Contents/Info.plist"
fi

if [ -n "$SIGN_IDENTITY" ]; then
  codesign --force --options runtime --timestamp --sign "$SIGN_IDENTITY" "$app"
else
  codesign --force --sign "${SIGN_ID:--}" "$app"
fi

if [ -n "$SIGN_IDENTITY" ] && [ -n "$NOTARY_KEY_ID" ]; then
  key=$(mktemp)
  trap 'rm -f "$key"' EXIT
  printf '%s' "$NOTARY_KEY_P8" > "$key"
  ditto -c -k --keepParent "$app" build/notarize.zip
  xcrun notarytool submit build/notarize.zip --key "$key" --key-id "$NOTARY_KEY_ID" \
    --issuer "$NOTARY_ISSUER_ID" --wait
  xcrun stapler staple "$app"
  rm build/notarize.zip
fi

if [ "$1" = --dmg ]; then
  stage=$(mktemp -d)
  cp -R "$app" "$stage/"
  ln -s /Applications "$stage/Applications"
  hdiutil create -volname Claudio -srcfolder "$stage" -ov -format UDZO build/Claudio.dmg
  rm -rf "$stage"
fi
echo "$app"
