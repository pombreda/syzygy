# Introduction #

Syzygy is all about taking apart perfectly good executable binaries, mutating them in interesting ways, and then outputting new, perfectly good executable binaries.
The output binaries are either functionally equivalent, but improved in some way - like e.g. in an improved order, or else contain new functionality, which is the case for instrumented binaries.

This is pretty exacting business, so correctness and quality are very important for Syzygy, hence these policies.

# Details #

## Contribution ##

If you don't work for Google and want to contribute code to Syzygy, you'll need to sign the [individual](http://code.google.com/legal/individual-cla-v1.0.html) or the [corporate](http://code.google.com/legal/corporate-cla-v1.0.html) contributor license.

Syzygy uses the same build tools as Chromium, so you'll need to set up the build environment as described in the [Chromium instructions](http://www.chromium.org/developers/how-tos/build-instructions-windows#TOC-Build-environment).  If you get build errors where 64-bit projects are skipped, it is likely because you failed to install "X64 Compilers and Tools" per the [Chromium instructions](http://www.chromium.org/developers/how-tos/build-instructions-windows#TOC-Build-environment).

Once this is done, you can fetch the sources by doing:
```
> mkdir Syzygy
> cd Syzygy
> C:\src\syzygy\foo>gclient config https://code.google.com/p/syzygy.git --name=src --unmanaged --deps-file=DEPS.syzygy
> gclient sync
```
After this completes, you can open the main solution file at `src\syzygy\syzygy.sln` and build it.

## Coding Guidelines ##

  1. Your code must conform to the Chromium style guidelines, and more generally to the Google C++ and Python style guides:
    * http://www.chromium.org/developers/coding-style
    * http://google-styleguide.googlecode.com/svn/trunk/cppguide.xml
    * https://google-styleguide.googlecode.com/svn/trunk/pyguide.html
  1. When in doubt follow the convention in use in a particular file or elsewhere in the repository.
  1. Your code should be tested by unit tests, and integration tests when appropriate. The Syzygy codebase is rigorously tested and observes strict coverage and testing standards. The continuous builder and integration tester is visible here:
> > http://build.chromium.org/p/client.syzygy/waterfall
  1. If you are contributing for the first time please take a moment to add your name to the AUTHORS.txt file in the root of the repository.
  1. All pre-submit checks must pass before committing the change. When uploading a CL these checks will be run automatically, and further directions provided. For more details see [review](#Review.md)
  1. All new code must be unittested. See the [testing](#Testing.md) section for details.

New submissions will trigger the continuous builder on [Syzygy's buildbot](http://build.chromium.org/p/client.syzygy/waterfall), which will compile the new contribution, run a suite of tests on it, and generate a code coverage report. Build or test failures on the buildbot should be fixed immediately, either by reverting the change, or by submitting a fix as soon as possible.

## Testing ##

Syzygy requires all C++ code to have better than 60% unittest coverage, as measured by the Syzygy [coverage builder](http://chromegw.corp.google.com/i/client.syzygy/builders/Syzygy%20Coverage).
After every submission, look at the next coverage report (you should get an email when it completes).
If the new code does not meet the coverage requirement, either revert the change, or submit a fix to improve the unittest coverage.

Any submission with a non-trivial bugfix must add a regression test for the bug.

Any significant feature addition must additionally be tested by an integration test, that tests the feature end-to-end.

## Review ##

Before being accepted into the codebase your contribution must be uploaded to
the Rietveld code review website at codereview.appspot.com, and reviewed by a
member of the Syzygy team. Try to find an appropriate reviewer by looking at the
most recent activity in that part of the repository. If in doubt contact the
team mailing list at syzygy-team@chromium.org.

Uploads to the review site are performed using "git cl upload" from the branch
containing your patch.

When your patch is accepted it will receive a "Looks good to me" (LGTM) from one
or more of your reviewers. At this point it is ready to commit.

## Commit ##

If this a one-off contribution it is easiest for a full-time team member to land
the CL for you. You will receive attribution in the commit message and the
AUTHORS.txt file.

If you have made or plan to make multiple contributions you will be added as a
contributor and provided with access to the repository. In this case you will
land your CL using "git cl land".

## Release criteria ##

TODO(siggi): release criteria.