#!/bin/bash
###########################################################################
#   Copyright (C) 2024-2024  by John McKisson - john.mckisson@gmail.com   #
#   Copyright (C) 2023-2025  by Stephen Lyons - slysven@virginmedia.com   #
#                                                                         #
#   This program is free software; you can redistribute it and/or modify  #
#   it under the terms of the GNU General Public License as published by  #
#   the Free Software Foundation; either version 2 of the License, or     #
#   (at your option) any later version.                                   #
#                                                                         #
#   This program is distributed in the hope that it will be useful,       #
#   but WITHOUT ANY WARRANTY; without even the implied warranty of        #
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
#   GNU General Public License for more details.                          #
#                                                                         #
#   You should have received a copy of the GNU General Public License     #
#   along with this program; if not, write to the                         #
#   Free Software Foundation, Inc.,                                       #
#   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             #
###########################################################################

set -x

# Version: 2.1.0    Remove MINGW32 since upstream no longer supports it
#          2.0.0    Rework to build on an MSYS2 MINGW64 Github workflow

# Exit codes:
# 0 - Everything is fine. 8-)
# 1 - Failure to change to a directory
# 2 - Unsupported fork
# 3 - Not used
# 4 - nuget error
# 5 - squirrel error

if [ "${MSYSTEM}" = "MSYS" ]; then
  echo "Please run this script from a MINGW64 type bash terminal as the MSYS one"
  echo "does not supported what is needed."
  exit 2
elif [ "${MSYSTEM}" = "MINGW64" ]; then
  export BUILDCOMPONENT="x86_64"
else
  echo "This script is not set up to handle systems of type ${MSYSTEM}, only"
  echo "MINGW64 is currently supported. Please rerun this in a bash terminal of"
  echo "that type."
  exit 2
fi

cd "${GITHUB_WORKSPACE}" || exit 1

# Add nuget location to PATH
PATH="/c/ProgramData/Chocolatey/bin:${PATH}"
export PATH

PUBLIC_TEST_BUILD="false"
# Check if GITHUB_REPO_TAG is "false"
if [[ "${GITHUB_REPO_TAG}" == "false" ]]; then
  echo "=== GITHUB_REPO_TAG is FALSE ==="

  # Check if this is a scheduled build
  if [[ "${GITHUB_SCHEDULED_BUILD}" == "true" ]]; then
    echo "=== GITHUB_SCHEDULED_BUILD is TRUE, this is a PTB ==="
    MUDLET_VERSION_BUILD="-ptb"
    # Now only exported
    PUBLIC_TEST_BUILD="true"
  else
    MUDLET_VERSION_BUILD="-testing"
  fi

  # Check if this is a pull request
  if [[ -n "${GITHUB_PULL_REQUEST_NUMBER}" ]]; then
    # Use the specific commit SHA from the pull request head, since GitHub Actions merges the PR
    BUILD_COMMIT=$(git rev-parse --short "${GITHUB_PULL_REQUEST_HEAD_SHA}")
    MUDLET_VERSION_BUILD="${MUDLET_VERSION_BUILD}-PR${GITHUB_PULL_REQUEST_NUMBER}"
  else
    BUILD_COMMIT=$(git rev-parse --short HEAD)

    if [[ "${MUDLET_VERSION_BUILD}" == "-ptb" ]]; then
      # Get current date in YYYY-MM-DD format
      DATE=$(date +%F)
      MUDLET_VERSION_BUILD="${MUDLET_VERSION_BUILD}-${DATE}"
    fi
  fi
fi

# Convert to lowercase, not all systems deal with uppercase ASCII characters
export MUDLET_VERSION_BUILD="${MUDLET_VERSION_BUILD,,}"
export BUILD_COMMIT="${BUILD_COMMIT,,}"

# Extract version from the mudlet.pro file
VersionLine=$(grep "VERSION =" "${GITHUB_WORKSPACE}/src/mudlet.pro")
VersionRegex='= {1}(.+)$'

# Use Bash regex matching to extract version
if [[ ${VersionLine} =~ ${VersionRegex} ]]; then
  VERSION="${BASH_REMATCH[1]}"
fi

# Check if MUDLET_VERSION_BUILD is empty and print accordingly
if [[ -z "${MUDLET_VERSION_BUILD}" ]]; then
  # Possible release build
  echo "BUILDING MUDLET ${VERSION}"
else
  # Include Git SHA1 in the build information
  echo "BUILDING MUDLET ${VERSION}${MUDLET_VERSION_BUILD}-${BUILD_COMMIT}"
fi

# Check if we're building from the Mudlet/Mudlet repository and not a fork
if [[ "${GITHUB_REPO_NAME}" != "Mudlet/Mudlet" ]]; then
  exit 2
fi

PACKAGE_DIR="$(cygpath -au "${GITHUB_WORKSPACE}/package-${MSYSTEM}-release")"

cd "${PACKAGE_DIR}" || exit 1

# Remove specific file types from the directory
rm ./*.cpp ./*.o

# Helper function to move a packaged mudlet to the upload directory and set up an artifact upload
# We require the files to be uploaded to exist in ${PACKAGE_DIR}
moveToUploadDir() {
  local uploadFilename=$1
  local UNZIP_INDEX=$2
  echo "=== Setting up upload directory ==="
  local UPLOAD_DIR="$(cygpath -au "${GITHUB_WORKSPACE}/upload")"

  # Make the upload directory if it doesn't exist
  mkdir -p "${UPLOAD_DIR}"

  echo "=== Copying files to upload directory ==="
  rsync -avR "${PACKAGE_DIR}"/./* "${UPLOAD_DIR}"

  # Append these variables to the GITHUB_ENV to make them available in subsequent steps
  # It is not clear that uploadFilename is used
  {
    echo "FOLDER_TO_UPLOAD=${UPLOAD_DIR}/"
    echo "uploadFilename=${UPLOAD_FILENAME}"
    echo "PARAM_UNZIP=${UNZIP_INDEX}"
  } >> "${GITHUB_ENV}"
}

# Check if GITHUB_REPO_TAG and GITHUB_SCHEDULED_BUILD are not "true" for a snapshot build
if [[ "${GITHUB_REPO_TAG}" != "true" ]] && [[ "${GITHUB_SCHEDULED_BUILD}" != "true" ]]; then
  echo "=== Creating a snapshot build ==="
  mv "${PACKAGE_DIR}/mudlet.exe" "Mudlet.exe"

  # Define the upload filename
  UPLOAD_FILENAME="Mudlet-${VERSION}${MUDLET_VERSION_BUILD}-${BUILD_COMMIT}-windows-64"

  # Move packaged files to the upload directory
  moveToUploadDir "${UPLOAD_FILENAME}" 0
else

  # Check if it's a Public Test Build
  if [[ "${GITHUB_SCHEDULED_BUILD}" == "true" ]]; then

    # Get the commit date of the last commit
    COMMIT_DATE=$(git show -s --format="%cs")
    # Get yesterday's date in the same format
    YESTERDAY_DATE=$(date --date="yesterday" +%Y-%m-%d)

    if [[ "${COMMIT_DATE}" < "${YESTERDAY_DATE}" ]]; then
      echo "=== No new commits, aborting public test build generation ==="
      exit 0
    fi

    echo "=== Creating a public test build ==="
    # Squirrel uses Start menu name from the binary, renaming it
    mv "${PACKAGE_DIR}/mudlet.exe" "${PACKAGE_DIR}/Mudlet PTB.exe"
    echo "moved mudlet.exe to ${PACKAGE_DIR}/Mudlet PTB.exe"
    # ensure sha part always starts with a character due to a known issue
    VERSION_AND_SHA="${VERSION}-ptb-${BUILD_COMMIT}"

  else
    echo "=== Creating a release build ==="
    mv "${PACKAGE_DIR}/mudlet.exe" "${PACKAGE_DIR}/Mudlet.exe"
    VERSION_AND_SHA="${VERSION}"
  fi

  echo "VERSION_AND_SHA: ${VERSION_AND_SHA}"
  echo "=== Cloning installer project ==="
  git clone https://github.com/Mudlet/installers.git "${GITHUB_WORKSPACE}/installers"
  cd "${GITHUB_WORKSPACE}/installers/windows" || exit 1

  echo "=== Setting up Java 21 for signing ==="
  # Java is installed by default, we just need to select which version to use:
  JAVA_HOME="$(cygpath -au "${JAVA_HOME_21_X64}")"
  export JAVA_HOME
  export PATH="${JAVA_HOME}/bin:${PATH}"

  if [ -z "${AZURE_ACCESS_TOKEN}" ]; then
      echo "=== Code signing skipped - no Azure token provided ==="
  else
      echo "=== Signing Mudlet and dll files ==="
      if [[ "${GITHUB_SCHEDULED_BUILD}" == "true" ]]; then
          java.exe -jar "${GITHUB_WORKSPACE}/installers/windows/jsign-7.0-SNAPSHOT.jar" --storetype TRUSTEDSIGNING \
              --keystore eus.codesigning.azure.net \
              --storepass "${AZURE_ACCESS_TOKEN}" \
              --alias Mudlet/Mudlet \
              "${PACKAGE_DIR}/Mudlet PTB.exe" "${PACKAGE_DIR}/**/*.dll"
      else
          java.exe -jar "${GITHUB_WORKSPACE}/installers/windows/jsign-7.0-SNAPSHOT.jar" --storetype TRUSTEDSIGNING \
              --keystore eus.codesigning.azure.net \
              --storepass "${AZURE_ACCESS_TOKEN}" \
              --alias Mudlet/Mudlet \
              "${PACKAGE_DIR}/Mudlet.exe" "${PACKAGE_DIR}/**/*.dll"
      fi
  fi

  echo "=== Installing Clowd.Squirrel for Windows ==="
  # nuget install squirrel.windows -ExcludeVersion
  # Although archived this is a replacement for the squirrel.windows original
  nuget install Clowd.Squirrel -ExcludeVersion -NonInteractive

  echo "=== Setting up directories ==="
  SQUIRREL_BASE_WINDIR="$(cygpath -aw "${GITHUB_WORKSPACE}/squirrel-packaging-prep/")"
  # Given that Windows 10 is the oldest Windows we now support NET 4.6.1 is
  # the new minimum we need to support (and the replacement Clowd.Squirrel does
  # not cover 4.5.x)
  SQUIRREL_WORKING_DIR="$(cygpath -au "${SQUIRREL_BASE_WINDIR}/lib/net461/")"
  # This will probably already exist from the installation of Clowd.Squirrel
  mkdir -p "${SQUIRREL_WORKING_DIR}"

  echo "=== Moving things to where Squirrel expects them ==="
  mv "${PACKAGE_DIR}/"* "${SQUIRREL_WORKING_DIR}"

  # Set the path to the nuspec file (we need both Windows and Unix formats):
  NUSPEC_PATHFILE="$(cygpath -au "${GITHUB_WORKSPACE}/installers/windows/mudlet.nuspec")"
  NUSPEC_WINPATHFILE="$(cygpath -aw "${NUSPEC_PATHFILE}")"
  echo "=== Creating Nuget package ==="

  # Rename the id and title for Squirrel
  if [[ "${GITHUB_SCHEDULED_BUILD}" == "true" ]]; then
    # Allow public test builds to be installed side by side with the release builds by renaming the app
    # No dots in the <id>: Guidelines by Squirrel
    sed -i "s/<id>Mudlet<\/id>/<id>Mudlet_64_-PublicTestBuild<\/id>/" "${NUSPEC_PATHFILE}"
    sed -i "s/<title>Mudlet<\/title>/<title>Mudlet x64 (Public Test Build)<\/title>/" "${NUSPEC_PATHFILE}"
  else
    sed -i "s/<id>Mudlet<\/id>/<id>Mudlet_64_<\/id>/" "${NUSPEC_PATHFILE}"
    sed -i "s/<title>Mudlet<\/title>/<title>Mudlet x64<\/title>/" "${NUSPEC_PATHFILE}"
  fi

  # Actually create the NuGet package
  nuget pack "${NUSPEC_WINPATHFILE}" -Version "${VERSION_AND_SHA}" -BasePath "${SQUIRREL_BASE_WINDIR}" -OutputDirectory "${SQUIRREL_BASE_WINDIR}"

  echo "=== Preparing to create installer ==="
  if [[ "${GITHUB_SCHEDULED_BUILD}" == "true" ]]; then
    NAME_SUFFIX="_64_-PublicTestBuild"
    INSTALLER_ICON_WINFILE="$(cygpath -aw "${GITHUB_WORKSPACE}/src/icons/mudlet_ptb.ico")"
    LOADING_GIF="$(cygpath -aw "${GITHUB_WORKSPACE}/installers/windows/splash-installing-ptb-2x.png")"
  else
    NAME_SUFFIX="_64_"
    INSTALLER_ICON_WINFILE="$(cygpath -aw "${GITHUB_WORKSPACE}/src/icons/mudlet.ico")"
    LOADING_GIF="$(cygpath -aw "${GITHUB_WORKSPACE}/installers/windows/splash-installing-2x.png")"
  fi

  # Always the case now - so incorporated in the above
  # Ensure 64 bit build is properly tagged
  # if [ "${MSYSTEM}" = "MINGW64" ]; then
  #   NAME_SUFFIX="_64_${NAME_SUFFIX}"
  # fi

  NUPKG_WINPATHFILE="$(cygpath -aw "${SQUIRREL_BASE_WINDIR}\\Mudlet${NAME_SUFFIX}.${VERSION_AND_SHA}.nupkg")"
  NUPKG_PATHFILE="$(cygpath -au "${NUPKG_WINPATHFILE}")"
  if [[ ! -f "${NUPKG_PATHFILE}" ]]; then
    echo "=== ERROR: ${NUPKG_PATHFILE} doesn't exist as expected! Build aborted ==="
    exit 4
  fi

  RELEASE_WINPATH="$(cygpath -aw "${GITHUB_WORKSPACE}/squirreloutput")"
  RELEASE_PATH="$(cygpath -au "${RELEASE_WINPATH}")"
  # Execute Squirrel to create the installer, since this is a Windows native
  # tool - supply it with Windows native paths:
  echo "=== Creating installers from Nuget package ==="
  # ./squirrel.windows/tools/Squirrel
  # Need to call a different executable since we are using a different package:
  ./Clowd.Squirrel/tools/Squirrel releasify "${NUPKG_WINPATHFILE}" \
    --releaseDir "${RELEASE_WINPATH}" \
    --loadingGif "${LOADING_GIF}" \
    --no-delta \
    --no-msi \
    --splashImage="${INSTALLER_ICON_WINFILE}"

  echo "=== Removing old directory content of release folder ==="
  rm -rf "${PACKAGE_DIR:?}/*"

  echo "=== Copying installer over ==="
  if [[ "${GITHUB_SCHEDULED_BUILD}" == "true" ]]; then
    INSTALLER_EXE_PATH="${PACKAGE_DIR}/Mudlet-${VERSION}${MUDLET_VERSION_BUILD}-${BUILD_COMMIT}-windows-64.exe"
  else # release
    INSTALLER_EXE_PATH="${PACKAGE_DIR}/Mudlet-${VERSION}-windows-64-installer.exe"
  fi
  INSTALLER_EXE_WINPATH="$(cygpath -aw "${INSTALLER_EXE_PATH}")"
  echo "INSTALLER_EXE_PATH: ${INSTALLER_EXE_PATH}"
  echo "Renaming \"${RELEASE_PATH}/Setup.exe\" to \"${INSTALLER_EXE_PATH}\""
  mv "${RELEASE_PATH}/Setup.exe" "${INSTALLER_EXE_PATH}"

  # Sign the final installer
  echo "=== Signing installer ==="
  java.exe -jar "${GITHUB_WORKSPACE}/installers/windows/jsign-7.0-SNAPSHOT.jar" --storetype TRUSTEDSIGNING \
      --keystore eus.codesigning.azure.net \
      --storepass "${AZURE_ACCESS_TOKEN}" \
      --alias Mudlet/Mudlet \
      "${INSTALLER_EXE_PATH}"

  # Check if the setup executable exists
  if [[ ! -f "${INSTALLER_EXE_PATH}" ]]; then
    echo "=== ERROR: Squirrel failed to generate the installer! Build aborted. Squirrel log is:"

    # Check if the SquirrelSetup.log exists and display its content
    # if [[ -f "./squirrel.windows/tools/SquirrelSetup.log" ]]; then
    if [[ -f "./Clowd.Squirrel/tools/SquirrelSetup.log" ]]; then
      echo "SquirrelSetup.log: "
      # cat "./squirrel.windows/tools/SquirrelSetup.log"
      cat "./Clowd.Squirrel/tools/SquirrelSetup.log"
    fi

    # Check if the Squirrel-Releasify.log exists and display its content
    # if [[ -f "./squirrel.windows/tools/Squirrel-Releasify.log" ]]; then
    if [[ -f "./Clowd.Squirrel/tools/Squirrel-Releasify.log" ]]; then
      echo "Squirrel-Releasify.log: "
      # cat "./squirrel.windows/tools/Squirrel-Releasify.log"
      cat "./Clowd.Squirrel/tools/Squirrel-Releasify.log"
    fi

    exit 5
  fi

  if [[ "${GITHUB_SCHEDULED_BUILD}" == "true" ]]; then
    echo "=== Uploading public test build to make.mudlet.org ==="

    UPLOAD_FILENAME="Mudlet-${VERSION}${MUDLET_VERSION_BUILD}-${BUILD_COMMIT}-windows-64-installer.exe"
    echo "UPLOAD_FILENAME: ${UPLOAD_FILENAME}"

    # Installer named ${UPLOAD_FILENAME} should exist in ${PACKAGE_DIR} now, we're ok to proceed
    moveToUploadDir "${UPLOAD_FILENAME}" 1
    # This identifies the "channel" that the release applies to, currently
    # we have three defined: this one; "release" and (unused) "testing":
    RELEASE_TAG="public-test-build"
    CHANGELOG_MODE="ptb"
  else

    echo "=== Uploading installer to https://www.mudlet.org/wp-content/files/?C=M;O=D ==="
    echo "${DEPLOY_SSH_KEY}" > temp_key_file

    # chown doesn't work in msys2 and scp requires the not be globally readable
    # use a powershell workaround to set the permissions correctly
    echo "Fixing permissions of private key file"
    powershell.exe -Command "icacls.exe temp_key_file /inheritance:r"

    powershell.exe <<EOF
\$installerExePath = "${INSTALLER_EXE_WINPATH}"
\$DEPLOY_PATH = "${DEPLOY_PATH}"
scp.exe -i temp_key_file -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \$installerExePath mudmachine@mudlet.org:\${DEPLOY_PATH}
EOF

    shred -u temp_key_file

    DEPLOY_URL="https://www.mudlet.org/wp-content/files/Mudlet-${VERSION}-windows-64-installer.exe"

    if ! curl --output /dev/null --silent --head --fail "${DEPLOY_URL}"; then
      echo "Error: release not found as expected at ${DEPLOY_URL}"
      exit 1
    fi

    SHA256SUM=$(shasum -a 256 "${INSTALLER_EXE_PATH}" | awk '{print $1}')

    current_timestamp=$(date "+%-d %-m %Y %-H %-M %-S")
    read -r day month year hour minute second <<< "${current_timestamp}"

    # blank echo to remove the stray 'PS D:\a\Mudlet\Mudlet\installers\windows> ' that shows up otherwise

    echo ""
    echo "=== Updating WP-Download-Manager ==="
    echo "sha256 of installer: ${SHA256SUM}"

    FILE_CATEGORY="2"

    current_timestamp=$(date "+%-d %-m %Y %-H %-M %-S")
    read -r day month year hour minute second <<< "${current_timestamp}"

    curl --retry 5 -X POST 'https://www.mudlet.org/download-add.php' \
    -H "x-wp-download-token: ${X_WP_DOWNLOAD_TOKEN}" \
    -F "file_type=2" \
    -F "file_remote=${DEPLOY_URL}" \
    -F "file_name=Mudlet ${VERSION} (windows-64)" \
    -F "file_des=sha256: ${SHA256SUM}" \
    -F "file_cat=${FILE_CATEGORY}" \
    -F "file_permission=-1" \
    -F "file_timestamp_day=${day}" \
    -F "file_timestamp_month=${month}" \
    -F "file_timestamp_year=${year}" \
    -F "file_timestamp_hour=${hour}" \
    -F "file_timestamp_minute=${minute}" \
    -F "file_timestamp_second=${second}" \
    -F "output=json" \
    -F "do=Add File"

    RELEASE_TAG="release"
    CHANGELOG_MODE="release"
  fi

  echo "=== Installing NodeJS ==="
  # Check: according to https://github.com/actions/runner-images/blob/main/images/windows/Windows2022-Readme.md
  # we already have node 22.17.1 available to us:
  choco install --no-progress nodejs --version="22.1.0" -y -r -n
  PATH="/c/Program Files/nodejs/:/c/npm/prefix/:${PATH}"
  export PATH

  echo "=== Installing dblsqd-cli ==="
  npm install -g dblsqd-cli
  dblsqd login -e "https://api.dblsqd.com/v1/jsonrpc" -u "${DBLSQD_USER}" -p "${DBLSQD_PASS}"

  echo "=== Downloading release feed ==="
  DOWNLOADED_FEED=$(mktemp)
  # We used to support both x86 and x86_64 and stored the current one in ARCH
  curl "https://feeds.dblsqd.com/MKMMR7HNSP65PquQQbiDIw/${RELEASE_TAG}/win/${ARCH}" -o "${DOWNLOADED_FEED}"

  echo "=== Generating a changelog ==="
  cd "${GITHUB_WORKSPACE}/CI" || exit 1

  CHANGELOG=$(lua5.1 "${GITHUB_WORKSPACE}/CI/generate-changelog.lua" --mode "${CHANGELOG_MODE}" --releasefile "${DownloadedFeed}")
  # cd - seems to swap between the current and previous working directory!
  cd - || exit 1
  echo "${CHANGELOG}"

  echo "=== Creating release in Dblsqd ==="
  if [[ "${GITHUB_SCHEDULED_BUILD}" == "true" ]]; then
    DBLSQD_VERSION_STRING="${VERSION}${MUDLET_VERSION_BUILD}-${BUILD_COMMIT,,}"
  else # release
    DBLSQD_VERSION_STRING="${VERSION}"
  fi
  
  echo "DBLSQD_VERSION_STRING: ${DBLSQD_VERSION_STRING}"
  export DBLSQD_VERSION_STRING

  # This may fail as a build from another architecture may have already registered a release with dblsqd,
  # if so, that is OK...
  echo "dblsqd release -a mudlet -c ${RELEASE_TAG} -m \"${Changelog}\" \"${DBLSQD_VERSION_STRING}\""
  dblsqd release -a mudlet -c "${RELEASE_TAG}" -m "${Changelog}" "${DBLSQD_VERSION_STRING}" || true

  # PTB's are handled by the register script, release builds are just pushed here
  if [[ "${RELEASE_TAG}" == "release" ]]; then
    echo "=== Registering release with Dblsqd ==="
    echo "dblsqd push -a mudlet -c release -r \"${DBLSQD_VERSION_STRING}\" -s mudlet --type 'standalone' --attach win:${ARCH} \"${DEPLOY_URL}\""
    dblsqd push -a mudlet -c release -r "${DBLSQD_VERSION_STRING}" -s mudlet --type 'standalone' --attach win:"${ARCH}" "${DEPLOY_URL}"
  fi

fi

# Make PUBLIC_TEST_BUILD available to GHA to check if we need to run the register step:
{
  echo "PUBLIC_TEST_BUILD=${PUBLIC_TEST_BUILD}"
  echo "ARCH=${ARCH}"
  echo "VERSION_STRING=${VersionString}"
  echo "BUILD_COMMIT=${BUILD_COMMIT}"
} >> "${GITHUB_ENV}"

echo ""
echo "******************************************************"
echo ""
if [[ -z "${MUDLET_VERSION_BUILD}" ]]; then
  # A release build
  echo "Finished building Mudlet ${VERSION}"
else
  # Not a release build so include the Git SHA1 in the message
  echo "Finished building Mudlet ${VERSION}${MUDLET_VERSION_BUILD}-${BUILD_COMMIT}"
fi

if [[ -n "${DEPLOY_URL}" ]]; then
  echo "Deployed the output to ${DEPLOY_URL}"
fi

echo ""
echo "******************************************************"
