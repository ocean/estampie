# On AI-assisted development

How ESTAMPIE is made, so you can decide for yourself whether to use it. This follows Leo Kuroshita's
[On AI-Assisted Development and Responsibility](https://github.com/hugelton/Felucca/discussions/166) for Felucca,
which ESTAMPIE is built on. Thank you, Leo, for writing it so clearly.

# My perspective in this instance

I can fully understand people objecting to AI-generated music, images, and other forms of art. They are entitled
to their opinions and I respect that fully. In general I do as well. Using an LLM to help write the software
for a machine that makes music is not the same thing.

For me it boils down to the fact that I am trying to deliver a quality experience with this device,
and I am hoping to empower people to use the FM-1 to make music to please themselves and others.

## What AI is used for

ESTAMPIE is developed with LLM-powered coding agents, mostly Anthropic's Claude (through Claude Code). They write
and change code, tests and documentation, do research (the FM-1's hardware, licences, other firmwares), and keep
the build and its supply chain in order. [AGENTS.md](../AGENTS.md) is the guide they work from.

## What AI is not used for

No generative AI model makes ESTAMPIE's music, sounds or pictures:

- **The tunes** are historical dances and pieces, centuries out of copyright, arranged into the FM-1's patterns.
- **The samples** are recordings of real instruments, released as CC0 by the people credited in
  [CREDITS.md](../CREDITS.md).
- **The sounds** are synthesis engines and presets: Felucca's, GHOULBOX's and ESTAMPIE's own.
- **The pictures** (the illuminated pages, the boot screen, the icons) are drawn by code, from shapes and
  colours chosen by hand, not by an image model. Some of the drawing code was written by agents.
- **The fonts** are published typefaces under their own licences ([LICENSES/](../LICENSES/)).

## Who is responsible

ESTAMPIE's concept, its features, how it sounds and looks, and what goes into each release are my decisions.
AI helps me to write the code, but the responsibility for ESTAMPIE is mine. Every change is tested (the host
tests, the simulator and the emulators), and each release is built and signed in the open on GitHub ([BUILDING.md](../BUILDING.md#releasing)), so anyone can check what they are installing.

ESTAMPIE also stands on other people's work, made with or without AI: Felucca, GHOULBOX, and everyone in
[CREDITS.md](../CREDITS.md). Their choices about AI are theirs.

## Forks

This is how I choose to be open about ESTAMPIE. It isn't a condition for anyone who forks this or builds
on it, just as Leo doesn't ask it of Felucca's forks.

Drew Robinson / @ocean
