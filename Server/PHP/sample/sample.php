<?php

declare(strict_types=1);

require_once __DIR__ . '/../lib/Display.php';
const DEVICE_FONT = 'helvetica';

function span(
	string $text,
	int $size,
	Color $color = Color::BLACK,
	FontWeight $weight = FontWeight::REGULAR,
	bool $underline = false,
	bool $strikeout = false,
	int $letter_spacing = 0
): TextSpan {
	return new TextSpan(
		text: $text,
		family: DEVICE_FONT,
		weight: $weight,
		size: $size,
		color: $color,
		letter_spacing: $letter_spacing,
		underline: $underline,
		strikeout: $strikeout
	);
}

$display = new Display();

$logo = new Imagick();
$logo->setBackgroundColor(new ImagickPixel('transparent'));
$logo->readImage(__DIR__ . '/../../../Media/logo.svg');
$logo->thumbnailImage(120, 0);

$display
	// Explicitly reset drawing transforms for this 792 x 272 layout.
	->setRotate(0)
	->setMirroring(0)
	->clear(Color::WHITE)
	->setPixel(12, 12, Color::RED)

	->text(new TextBox(
		x: 20,
		y: 10,
		width: 752,
		height: 42,
		spans: [span('JSON Paper · Rich text', 28, Color::WHITE, FontWeight::BOLD)],
		background: Color::BLACK,
		horizontal_align: HorizontalAlign::CENTER,
		vertical_align: VerticalAlign::MIDDLE
	))

	->clearWindow(24, 66, 84, 112, Color::YELLOW)
	->point(108, 89, Color::BLACK, Width::W4, PointStyle::AROUND)
	->line(132, 68, 218, 110, Color::BLACK, Width::W2, LineStyle::DOTTED)
	->rectangle(238, 68, 316, 110, Color::RED, Width::W2, FillMode::EMPTY)
	->rectangle(328, 78, 378, 102, Color::BLACK, Width::W1, FillMode::FULL)
	->circle(420, 89, 22, Color::YELLOW, Width::W2, FillMode::FULL)
	->circle(474, 89, 22, Color::RED, Width::W2, FillMode::EMPTY)
	->pieSlice(530, 89, 24, 0, 120, Color::RED)
	->pieSlice(530, 89, 24, 120, 120, Color::YELLOW)
	->pieSlice(530, 89, 24, 240, 120, Color::BLACK)
	->image(584, 66, $logo, Color::TRANSPARENT)

	->text(new TextBox(
		x: 24,
		y: 132,
		width: 350,
		height: 112,
		spans: [
			span("Temperature\n", 16),
			span('21 ', 42, Color::RED, FontWeight::BOLD),
			span('°C', 26, Color::RED),
		],
		background: Color::YELLOW,
		horizontal_align: HorizontalAlign::CENTER,
		vertical_align: VerticalAlign::MIDDLE,
		line_spacing: 4
	))

	->text(new TextBox(
		x: 398,
		y: 132,
		width: 370,
		height: 112,
		spans: [
			span('MESSAGE', 18, Color::BLACK, FontWeight::BOLD, underline: true, letter_spacing: 2),
			span("\nRødgrød med fløde · ÆØÅ æøå\n", 17),
			span('14:30', 17, strikeout: true),
			span('  Updated to 15:00', 17, Color::RED),
		],
		background: Color::WHITE,
		horizontal_align: HorizontalAlign::LEFT,
		vertical_align: VerticalAlign::MIDDLE,
		line_spacing: 3,
		overflow: TextOverflow::CLIP
	))

	->text(new TextBox(
		x: 24,
		y: 248,
		width: 350,
		height: 20,
		spans: [span('Native text · right / bottom aligned', 12)],
		horizontal_align: HorizontalAlign::RIGHT,
		vertical_align: VerticalAlign::BOTTOM,
		overflow: TextOverflow::CLIP
	));

// The raster fallback currently supports simple single-span text. Use text()
// above for mixed spans, alignment, wrapping, spacing, and decorations.
$display->rasterText(
	new TextBox(398, 250, 370, 20, [span('Server-rendered text · 21 °C', 12)]),
	new RasterFontFamily(
		__DIR__ . '/../../../Arduino/src/font/assets/Helvetica-Regular.ttf',
		__DIR__ . '/../../../Arduino/src/font/assets/Helvetica-Bold.ttf'
	)
);

$display->output();
