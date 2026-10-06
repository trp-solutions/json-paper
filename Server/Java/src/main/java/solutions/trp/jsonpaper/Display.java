package solutions.trp.jsonpaper;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;

import java.awt.image.BufferedImage;
import java.io.IOException;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;

public final class Display {
    public static final String VERSION = "2.0";
    public static final int MAX_DOCUMENT_BYTES = 128 * 1024;

    private static final ObjectMapper OBJECT_MAPPER = new ObjectMapper();

    private final List<DisplayCommand> commands = new ArrayList<>();

    public Display setRotate(int rotate) {
        if (rotate != 0 && rotate != 90 && rotate != 180 && rotate != 270) {
            throw new IllegalArgumentException("rotation must be 0, 90, 180, or 270 degrees");
        }
        return add("set_rotate", args("rotate", rotate));
    }

    /** Mirror modes: 0 none, 1 horizontal, 2 vertical, 3 both axes. */
    public Display setMirroring(int mirror) {
        if (mirror < 0 || mirror > 3) {
            throw new IllegalArgumentException("mirror must be 0-3");
        }
        return add("set_mirroring", args("mirror", mirror));
    }

    public Display setPixel(int x, int y) {
        return setPixel(x, y, Color.BLACK);
    }

    public Display setPixel(int x, int y, Color color) {
        if (color == Color.TRANSPARENT) {
            throw new IllegalArgumentException("pixel color cannot be transparent");
        }
        return add("set_pixel", args("x", x, "y", y, "color", color));
    }

    public Display clear(Color color) {
        return add("clear", args(
            "color", color
        ));
    }

    public Display clearWindow(
        int xStart,
        int yStart,
        int xEnd,
        int yEnd,
        Color color
    ) {
        return add("clear_window", args(
            "x_start", xStart,
            "y_start", yStart,
            "x_end", xEnd,
            "y_end", yEnd,
            "color", color
        ));
    }

    public Display point(int x, int y) {
        return point(x, y, Color.BLACK, Width.W1, PointStyle.AROUND);
    }

    public Display point(
        int x,
        int y,
        Color color,
        Width width,
        PointStyle style
    ) {
        return add("draw_point", args(
            "x", x,
            "y", y,
            "color", color,
            "width", width,
            "style", style
        ));
    }

    public Display line(int xStart, int yStart, int xEnd, int yEnd) {
        return line(
            xStart, yStart, xEnd, yEnd,
            Color.BLACK, Width.W1, LineStyle.SOLID
        );
    }

    public Display line(
        int xStart,
        int yStart,
        int xEnd,
        int yEnd,
        Color color,
        Width width,
        LineStyle style
    ) {
        return add("draw_line", args(
            "x_start", xStart,
            "y_start", yStart,
            "x_end", xEnd,
            "y_end", yEnd,
            "color", color,
            "width", width,
            "style", style
        ));
    }

    public Display rectangle(int xStart, int yStart, int xEnd, int yEnd) {
        return rectangle(
            xStart, yStart, xEnd, yEnd,
            Color.BLACK, Width.W1, FillMode.EMPTY
        );
    }

    public Display rectangle(
        int xStart,
        int yStart,
        int xEnd,
        int yEnd,
        Color color,
        Width width,
        FillMode fill
    ) {
        return add("draw_rectangle", args(
            "x_start", xStart,
            "y_start", yStart,
            "x_end", xEnd,
            "y_end", yEnd,
            "color", color,
            "width", width,
            "fill", fill
        ));
    }

    public Display circle(int x, int y, int radius) {
        return circle(
            x, y, radius,
            Color.BLACK, Width.W1, FillMode.EMPTY
        );
    }

    public Display circle(
        int x,
        int y,
        int radius,
        Color color,
        Width width,
        FillMode fill
    ) {
        return add("draw_circle", args(
            "x", x,
            "y", y,
            "radius", radius,
            "color", color,
            "width", width,
            "fill", fill
        ));
    }

    public Display pieSlice(
        int x,
        int y,
        int radius,
        int startAngle,
        int sweepAngle,
        Color color
    ) {
        return add("draw_pie_slice", args(
            "x", x,
            "y", y,
            "radius", radius,
            "start_angle", startAngle,
            "sweep_angle", sweepAngle,
            "color", color
        ));
    }

    public Display text(TextBox textBox) {
        Objects.requireNonNull(textBox, "textBox");
        return add("draw_text", OBJECT_MAPPER.convertValue(textBox,
            OBJECT_MAPPER.getTypeFactory().constructMapType(LinkedHashMap.class, String.class, Object.class)));
    }

    public Display rasterText(TextBox textBox, RasterFontFamily fonts) {
        Objects.requireNonNull(textBox, "textBox");
        return image(textBox.x(), textBox.y(), RasterTextRenderer.render(textBox, fonts), Color.TRANSPARENT);
    }

    public Display image(int x, int y, BufferedImage image) {
        return image(x, y, image, Color.TRANSPARENT);
    }

    public Display image(
        int x,
        int y,
        BufferedImage image,
        Color transparent
    ) {
        return image(x, y, PaperImage.from(image), transparent);
    }

    public Display image(int x, int y, Path path) throws IOException {
        return image(x, y, ImageLoader.load(path), Color.TRANSPARENT);
    }

    public Display image(
        int x,
        int y,
        Path path,
        Color transparent
    ) throws IOException {
        return image(x, y, ImageLoader.load(path), transparent);
    }

    public Display image(
        int x,
        int y,
        Path path,
        int width,
        int height,
        Color transparent
    ) throws IOException {
        return image(x, y, ImageLoader.load(path, width, height), transparent);
    }

    public Display image(int x, int y, PaperImage image, Color transparent) {
        Objects.requireNonNull(image, "image");
        Objects.requireNonNull(transparent, "transparent");

        return add("draw_image", args(
            "x", x,
            "y", y,
            "width", image.width(),
            "height", image.height(),
            "data", image.base64Data(),
            "transparent", transparent.paletteIndex()
        ));
    }

    public DisplayDocument document() {
        return new DisplayDocument(VERSION, commands);
    }

    public String toJson() {
        try {
            return checkedJson(false);
        } catch (JsonProcessingException exception) {
            throw new IllegalStateException("Could not serialize display document", exception);
        }
    }

    public String toPrettyJson() {
        try {
            return checkedJson(true);
        } catch (JsonProcessingException exception) {
            throw new IllegalStateException("Could not serialize display document", exception);
        }
    }

    private String checkedJson(boolean pretty) throws JsonProcessingException {
        String json = pretty
            ? OBJECT_MAPPER.writerWithDefaultPrettyPrinter().writeValueAsString(document())
            : OBJECT_MAPPER.writeValueAsString(document());
        if (json.getBytes(java.nio.charset.StandardCharsets.UTF_8).length > MAX_DOCUMENT_BYTES) {
            throw new IllegalStateException("display document exceeds the firmware 128 KB limit");
        }
        return json;
    }

    private Display add(String command, Map<String, Object> arguments) {
        commands.add(new DisplayCommand(command, arguments));
        return this;
    }

    private static Map<String, Object> args(Object... entries) {
        if (entries.length % 2 != 0) {
            throw new IllegalArgumentException("Arguments must be key/value pairs");
        }

        Map<String, Object> arguments = new LinkedHashMap<>();
        for (int index = 0; index < entries.length; index += 2) {
            String key = (String) entries[index];
            Object value = Objects.requireNonNull(entries[index + 1], key);
            if (value instanceof WireValue wireValue) {
                value = wireValue.value();
            }
            arguments.put(key, value);
        }
        return arguments;
    }
}
