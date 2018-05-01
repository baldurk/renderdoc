if [[ "$DOCS_BUILD" == "1" ]]; then

	echo "== Setting up documentation build.";

	. ./util/travis/docs_setup.sh;

else

	echo "== Setting up code build.";

	if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then

		if [[ "$LINUX_BUILD" == "1" ]]; then

			. ./util/travis/linux_setup.sh;

		elif [[ "$ANDROID_BUILD" == "1" ]]; then

			. ./util/travis/android_setup.sh;

		else

			echo "Unknown configuration building on linux - not targetting linux or android.";
			exit 1;

		fi

	elif [[ "$TRAVIS_OS_NAME" == "osx" ]]; then

		if [[ "$APPLE_BUILD" == "1" ]]; then

			. ./util/travis/osx_setup.sh

		else

			echo "Unknown configuration building on OSX - not targetting OSX.";
			exit 1;

		fi

	else

		echo "Unknown travis OS: $TRAVIS_OS_NAME.";
		exit 1;

	fi

fi
