# krosshair
A crosshair overlay for games on linux using vulkan.

![max](img/maxterOk-crosshair.png)
![quake](img/quake-crosshair.png)

## Installation

### AUR

```bash
yay -S krosshair
```

### From source

```bash
git clone https://github.com/fibsussy/krosshair.git
cd krosshair
make
```

## Usage

```bash
export KROSSHAIR=1
your-game
```

For Steam games, add `KROSSHAIR=1 %command%` to launch options.

## crosshair-maker integration

krosshair works out of the box with [crosshair-maker](https://github.com/fibsussy/crosshair-maker), a crosshair overlay creator with SVG rendering and preview. The currently selected crosshair is automatically exported to `~/.config/crosshair-maker/projects/current.png`, which krosshair picks up as its default — just launch your game with `export KROSSHAIR=1` and go.

You can override the image with `export KROSSHAIR_IMG=/path/to/crosshair.png` if you want to use a custom file.

## Can i get banned for this?
I don't know, use at your own risk. I've only used it in Quake Champions and STRAFTAT, both of which don't really have an anticheat.

## Issues
As of now, the overlay leaks a bit of memory everytime you alt-tab out of/into the game, as well as everytime the window is being resized and upon resolution changes. It's not a big leak and shouldn't cause any problems, but it's still worth noting.
I've tried fixing it multiple times but have always hit a dead-end. If someone more experienced with vulkan wants to help, take a look at [this issue](https://github.com/krob64/krosshair/issues/1). I know it's a bit of a mess, this whole project is based on a morally questionable apex legends project which i'm unsure if i should link to it here, coupled with me jumping into it right after completing the vulkan tutorial.
