<?php
require_once __DIR__."/../lib/Display.php";

const DEVICE_FONT = 'helvetica';

$display = new Display();

$img = new \Imagick();

$img->setResolution(30, 30);

$img->setOption('svg:antialias', 'false');
$img->setOption('svg:shape-rendering', 'crispEdges');

$img->setBackgroundColor(new \ImagickPixel('transparent'));

$img->readImage(__DIR__ . '/../../../Media/logo.svg');


$todo_task_count = 5;
$awaiting_task_count = 5;
$finished_task_count = 5;

$mon_1_work_hours = 3;
$tue_1_work_hours = 12;
$wed_1_work_hours = 6;
$thu_1_work_hours = 1;
$fri_1_work_hours = 2;

$mon_2_work_hours = 7;
$tue_2_work_hours = 3;
$wed_2_work_hours = 4;
$thu_2_work_hours = 5;
$fri_2_work_hours = 5;

$total_task_count = $todo_task_count + $awaiting_task_count + $finished_task_count;
$display
	->clear(Color::WHITE)

	->image(
		x: 5,
		y: 5,
		img: $img,
		transparent: Color::TRANSPARENT
	)

	->pieSlice(
		x: 85,
		y: 185,
		radius: 70,
		start_angle: 0,
		sweep_angle: 360*($todo_task_count / $total_task_count),
		color: Color::RED
	)

	->pieSlice(
		x: 85,
		y: 185,
		radius: 70,
		start_angle: 360*($todo_task_count / $total_task_count),
		sweep_angle: 360*($awaiting_task_count / $total_task_count),
		color: Color::YELLOW
	)

	->pieSlice(
		x: 85,
		y: 185,
		radius: 70,
		start_angle: 360*($todo_task_count / $total_task_count) + 360*($awaiting_task_count / $total_task_count),
		sweep_angle: 360*($finished_task_count / $total_task_count),
		color: Color::BLACK
	)

	->circle(
		x: 175,
		y: 157,
		radius: 7,
		color: Color::RED,
		fill: FillMode::FULL
	)
	->text(new TextBox(195, 150, 100, 24, [new TextSpan("Todo", family: DEVICE_FONT, color: Color::RED)]))

	->circle(
		x: 175,
		y: 187,
		radius: 7,
		color: Color::YELLOW,
		fill: FillMode::FULL
	)

	->text(new TextBox(195, 180, 110, 24, [new TextSpan("Awaiting", family: DEVICE_FONT, color: Color::YELLOW)]))

	->circle(
		x: 175,
		y: 217,
		radius: 7,
		color: Color::BLACK,
		fill: FillMode::FULL
	)

	->text(new TextBox(195, 210, 110, 24, [new TextSpan("Finished", family: DEVICE_FONT)]));

$week_1_work_hours = [
	$mon_1_work_hours,
	$tue_1_work_hours,
	$wed_1_work_hours,
	$thu_1_work_hours,
	$fri_1_work_hours,
];

$week_2_work_hours = [
	$mon_2_work_hours,
	$tue_2_work_hours,
	$wed_2_work_hours,
	$thu_2_work_hours,
	$fri_2_work_hours,
];

$work_days = ["MON", "TUE", "WED", "THU", "FRI"];
$work_hours = [...$week_1_work_hours, ...$week_2_work_hours];
$work_day_labels = [...$work_days, ...$work_days];
$work_colors = [
	...array_fill(0, count($work_days), Color::RED),
	...array_fill(0, count($work_days), Color::YELLOW),
];

$chart_left = 320;
$chart_top = 10;
$chart_right = 780;
$chart_bottom = 160;
$chart_inner_height = $chart_bottom - $chart_top - 2;
$bar_slot_width = intdiv($chart_right - $chart_left, count($work_hours));
$bar_width = $bar_slot_width - 4;
$max_work_hours = max(1, (int) ceil(max(...$work_hours)));

$display->rectangle(
	x_start: $chart_left,
	y_start: $chart_top,
	x_end: $chart_right,
	y_end: $chart_bottom,
	color: Color::BLACK
);

$tick_step = max(1, (int) ceil($max_work_hours / 6));
$tick_values = range($tick_step, $max_work_hours, $tick_step);
if (end($tick_values) !== $max_work_hours) {
	$tick_values[] = $max_work_hours;
}

foreach ($tick_values as $hour) {
	$tick_y = $chart_bottom - (int) round(
		$chart_inner_height * $hour / $max_work_hours
	);
	$tick_text = (string) $hour;

	$display->text(new TextBox(
		$chart_left - 40, max($chart_top, min($chart_bottom - 12, $tick_y - 6)),
		34, 18, [new TextSpan($tick_text, family: DEVICE_FONT, size: 12)],
		horizontal_align: HorizontalAlign::RIGHT
	));

	if ($hour !== $max_work_hours) {
		$display->line(
			x_start: $chart_left,
			y_start: $tick_y,
			x_end: $chart_right,
			y_end: $tick_y,
			color: Color::BLACK,
			width: Width::W1,
			style: LineStyle::DOTTED
		);
	}
}

foreach ($work_hours as $bar_index => $hours) {
	$bar_center = $chart_left
		+ $bar_index * $bar_slot_width
		+ intdiv($bar_slot_width, 2);
	$bar_height = (int) round(
		$chart_inner_height * $hours / $max_work_hours
	);
	$bar_color = $work_colors[$bar_index];

	if ($bar_height > 0) {
		$display->rectangle(
			x_start: $bar_center - intdiv($bar_width, 2),
			y_start: $chart_bottom - $bar_height,
			x_end: $bar_center + intdiv($bar_width, 2) - 1,
			y_end: $chart_bottom,
			color: $bar_color,
			fill: FillMode::FULL
		);
	}

	$display->text(new TextBox($bar_center - 20, 165, 40, 18,
		[new TextSpan($work_day_labels[$bar_index], family: DEVICE_FONT, size: 12)],
		horizontal_align: HorizontalAlign::CENTER));
}

$total_work_hours = array_sum($work_hours);
$total_projects = $total_task_count;

$display
	->text(new TextBox(320, 210, 300, 24,
		[new TextSpan("Total hours spend: " . $total_work_hours, family: DEVICE_FONT)]))
	->text(new TextBox(320, 240, 300, 24,
		[new TextSpan("Total Projects: " . $total_projects, family: DEVICE_FONT)]));

$display->output();
