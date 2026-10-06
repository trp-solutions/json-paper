<?php

require_once __DIR__ . "/../lib/image.php";

enum Color: string
{
	case BLACK = "black";
	case WHITE = "white";
	case YELLOW = "yellow";
	case RED = "red";
	case TRANSPARENT = "transparent";
}

enum FontWeight: string { case REGULAR = "regular"; case BOLD = "bold"; }
enum HorizontalAlign: string { case LEFT = "left"; case CENTER = "center"; case RIGHT = "right"; }
enum VerticalAlign: string { case TOP = "top"; case MIDDLE = "middle"; case BOTTOM = "bottom"; }
enum TextOverflow: string { case CLIP = "clip"; case ELLIPSIS = "ellipsis"; }

final readonly class TextSpan implements JsonSerializable
{
	public function __construct(
		public string $text,
		public string $family = "helvetica",
		public FontWeight $weight = FontWeight::REGULAR,
		public int $size = 16,
		public Color $color = Color::BLACK,
		public int $letter_spacing = 0,
		public bool $underline = false,
		public bool $strikeout = false
	) {
		if ($family === '') throw new InvalidArgumentException('family must not be empty');
		if ($size < 6 || $size > 96) throw new InvalidArgumentException('size must be 6-96 pixels');
		if ($color === Color::TRANSPARENT) throw new InvalidArgumentException('text color cannot be transparent');
	}
	public function jsonSerialize(): array { return get_object_vars($this); }
}

final readonly class TextBox implements JsonSerializable
{
	/** @param list<TextSpan> $spans */
	public function __construct(
		public int $x,
		public int $y,
		public int $width,
		public int $height,
		public array $spans,
		public Color $background = Color::TRANSPARENT,
		public HorizontalAlign $horizontal_align = HorizontalAlign::LEFT,
		public VerticalAlign $vertical_align = VerticalAlign::TOP,
		public string $wrap = 'word',
		public TextOverflow $overflow = TextOverflow::ELLIPSIS,
		public int $line_spacing = 0
	) {
		if ($width <= 0 || $height <= 0) throw new InvalidArgumentException('text box width and height must be positive');
		if ($spans === []) throw new InvalidArgumentException('at least one text span is required');
		foreach ($spans as $span) if (!$span instanceof TextSpan) throw new InvalidArgumentException('spans must contain TextSpan values');
		if ($wrap !== 'word') throw new InvalidArgumentException('only word wrapping is supported');
		if ($line_spacing < 0) throw new InvalidArgumentException('line spacing must not be negative');
	}
	public function jsonSerialize(): array { return get_object_vars($this); }
}

final readonly class RasterFontFamily
{
	public function __construct(public string $regular, public string $bold) {
		if ($regular === '' || $bold === '') throw new InvalidArgumentException('font paths must not be empty');
	}
}

enum Width: string
{
	case W1 = "1x1";
	case W2 = "2x2";
	case W3 = "3x3";
	case W4 = "4x4";
}

enum LineStyle: string
{
	case SOLID = "solid";
	case DOTTED = "dotted";
}

enum PointStyle: string
{
	case AROUND = "around";
	case RIGHTUP = "rightup";
}

enum FillMode: string
{
	case EMPTY = "empty";
	case FULL = "full";
}
class Display
{
	private array $commands = [];

	public function setRotate(int $rotate): self
	{
		if (!in_array($rotate, [0, 90, 180, 270], true)) {
			throw new InvalidArgumentException('rotation must be 0, 90, 180, or 270 degrees');
		}
		$this->commands[] = ['cmd' => 'set_rotate', 'args' => ['rotate' => $rotate]];
		return $this;
	}

	/** 0: none, 1: horizontal, 2: vertical, 3: both axes. */
	public function setMirroring(int $mirror): self
	{
		if ($mirror < 0 || $mirror > 3) {
			throw new InvalidArgumentException('mirror must be 0-3');
		}
		$this->commands[] = ['cmd' => 'set_mirroring', 'args' => ['mirror' => $mirror]];
		return $this;
	}

	public function setPixel(int $x, int $y, Color $color = Color::BLACK): self
	{
		if ($color === Color::TRANSPARENT) {
			throw new InvalidArgumentException('pixel color cannot be transparent');
		}
		$this->commands[] = ['cmd' => 'set_pixel', 'args' => [
			'x' => $x, 'y' => $y, 'color' => $color,
		]];
		return $this;
	}

	private function colorToIndex(
		Color $color
	): int {

		return match ($color) {
			Color::BLACK => 0,
			Color::WHITE => 1,
			Color::YELLOW => 2,
			Color::RED => 3,
			Color::TRANSPARENT => 4,
		};
	}

	public function clear(Color $color): self
	{
		$this->commands[] = [
			"cmd" => "clear",
			"args" => [
				"color" => $color
			]
		];

		return $this;
	}

	public function clearWindow(
		int   $x_start,
		int   $y_start,
		int   $x_end,
		int   $y_end,
		Color $color
	): self {

		$this->commands[] = [
			"cmd" => "clear_window",
			"args" => [
				"x_start" => $x_start,
				"y_start" => $y_start,
				"x_end" => $x_end,
				"y_end" => $y_end,
				"color" => $color
			]
		];

		return $this;
	}

	public function point(
		int $x,
		int $y,
		Color $color = Color::BLACK,
		Width $width = Width::W1,
		PointStyle $style = PointStyle::AROUND
	): self {

		$this->commands[] = [
			"cmd" => "draw_point",
			"args" => [
				"x" => $x,
				"y" => $y,
				"color" => $color,
				"width" => $width,
				"style" => $style
			]
		];

		return $this;
	}

	public function line(
		int       $x_start,
		int       $y_start,
		int       $x_end,
		int       $y_end,
		Color     $color = Color::BLACK,
		Width     $width = Width::W1,
		LineStyle $style = LineStyle::SOLID
	): self {

		$this->commands[] = [
			"cmd" => "draw_line",
			"args" => [
				"x_start" => $x_start,
				"y_start" => $y_start,
				"x_end" => $x_end,
				"y_end" => $y_end,
				"color" => $color,
				"width" => $width,
				"style" => $style
			]
		];

		return $this;
	}

	public function rectangle(
		int      $x_start,
		int      $y_start,
		int      $x_end,
		int      $y_end,
		Color    $color = Color::BLACK,
		Width    $width = Width::W1,
		FillMode $fill = FillMode::EMPTY
	): self {

		$this->commands[] = [
			"cmd" => "draw_rectangle",
			"args" => [
				"x_start" => $x_start,
				"y_start" => $y_start,
				"x_end" => $x_end,
				"y_end" => $y_end,
				"color" => $color,
				"width" => $width,
				"fill" => $fill
			]
		];

		return $this;
	}

	public function circle(
		int      $x,
		int      $y,
		int      $radius,
		Color    $color = Color::BLACK,
		Width    $width = Width::W1,
		FillMode $fill = FillMode::EMPTY
	): self {

		$this->commands[] = [
			"cmd" => "draw_circle",
			"args" => [
				"x" => $x,
				"y" => $y,
				"radius" => $radius,
				"color" => $color,
				"width" => $width,
				"fill" => $fill
			]
		];

		return $this;
	}

	public function pieSlice(
		int   $x,
		int   $y,
		int   $radius,
		int   $start_angle,
		int   $sweep_angle,
		Color $color
	): self {

		$this->commands[] = [
			"cmd" => "draw_pie_slice",
			"args" => [
				"x" => $x,
				"y" => $y,
				"radius" => $radius,
				"start_angle" => $start_angle,
				"sweep_angle" => $sweep_angle,
				"color" => $color
			]
		];

		return $this;
	}

	public function text(TextBox $box): self {

		$this->commands[] = [
			"cmd" => "draw_text",
			"args" => $box
		];

		return $this;
	}

	public function rasterText(TextBox $box, RasterFontFamily $fonts): self
	{
		if (!extension_loaded('imagick')) throw new RuntimeException('Imagick is required for rasterText');
		$image = new Imagick();
		$image->newImage($box->width, $box->height, new ImagickPixel('transparent'), 'png');
		if ($box->background !== Color::TRANSPARENT) {
			$image->setImageBackgroundColor(new ImagickPixel($box->background->value));
			$image->setImageAlphaChannel(Imagick::ALPHACHANNEL_REMOVE);
		}
		$draw = new ImagickDraw();
		$y = 0;
		foreach ($box->spans as $span) {
			$draw->setFont($span->weight === FontWeight::BOLD ? $fonts->bold : $fonts->regular);
			$draw->setFontSize($span->size);
			$draw->setFillColor(new ImagickPixel($span->color->value));
			$metrics = $image->queryFontMetrics($draw, $span->text);
			$y = max($y, (int)ceil($metrics['ascender']));
			$draw->annotation(0, $y, str_replace(["\r", "\t"], ['', '    '], $span->text));
		}
		$image->drawImage($draw);
		return $this->image($box->x, $box->y, $image, Color::TRANSPARENT);
	}

	public function image(
		int      $x,
		int      $y,
		\Imagick $img,
		Color    $transparent = Color::TRANSPARENT
	): self {

		$converted = new Image($img);

		$this->commands[] = [
			"cmd" => "draw_image",
			"args" => [
				"x" => $x,
				"y" => $y,
				"width" => $converted->width,
				"height" => $converted->height,
				"data" => base64_encode(
					$converted->getRawData()
				),
				"transparent" => $this->colorToIndex(
					$transparent
				)
			]
		];

		return $this;
	}

	public function array(): array
	{
		return [
			"version" => "2.0",
			"commands" => $this->commands
		];
	}

	public function json(
		int $flags = JSON_PRETTY_PRINT
	): string {

		$json = json_encode(
			$this->array(),
			$flags | JSON_THROW_ON_ERROR
		);
		if (strlen($json) > 128 * 1024) throw new LengthException('display document exceeds the firmware 128 KB limit');
		return $json;
	}

	public function output(): void
	{
		header('Content-Type: application/json');

		echo $this->json();
	}
}
