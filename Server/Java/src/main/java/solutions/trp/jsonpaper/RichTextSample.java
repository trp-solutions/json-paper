package solutions.trp.jsonpaper;

import java.awt.Graphics2D;
import java.awt.image.BufferedImage;
import java.io.IOException;
import java.nio.file.Path;
import java.util.List;

/** A self-contained v2 sample matching Server/PHP/sample/sample.php. */
public final class RichTextSample {
    private static final String DEVICE_FONT = "helvetica";

    private RichTextSample() {
    }

    public static DisplayDocument create() {
        BufferedImage icon = new BufferedImage(120, 36, BufferedImage.TYPE_INT_ARGB);
        Graphics2D graphics = icon.createGraphics();
        graphics.setColor(java.awt.Color.BLACK);
        graphics.fillRect(0, 0, 120, 36);
        graphics.setColor(java.awt.Color.WHITE);
        graphics.fillRect(4, 4, 112, 28);
        graphics.setColor(java.awt.Color.RED);
        graphics.fillOval(46, 4, 28, 28);
        graphics.dispose();
        return create(icon);
    }

    public static DisplayDocument create(Path imagePath) throws IOException {
        return create(ImageLoader.load(imagePath, 120, 36));
    }

    public static DisplayDocument create(BufferedImage image) {
        return display(image).document();
    }

    /** Adds a server-rendered footer using caller-supplied font files. */
    public static DisplayDocument create(BufferedImage image, RasterFontFamily fonts) {
        return display(image)
            .rasterText(TextBox.builder(398, 250, 370, 20)
                .span(span("Server-rendered text · 21 °C", 12, Color.BLACK, FontWeight.REGULAR))
                .build(), fonts)
            .document();
    }

    private static Display display(BufferedImage image) {
        return new Display()
            .setRotate(0)
            .setMirroring(0)
            .clear(Color.WHITE)
            .setPixel(12, 12, Color.RED)
            .text(TextBox.builder(20, 10, 752, 42)
                .background(Color.BLACK)
                .horizontalAlign(HorizontalAlign.CENTER)
                .verticalAlign(VerticalAlign.MIDDLE)
                .span(span("JSON Paper · Rich text", 28, Color.WHITE, FontWeight.BOLD))
                .build())
            .clearWindow(24, 66, 84, 112, Color.YELLOW)
            .point(108, 89, Color.BLACK, Width.W4, PointStyle.AROUND)
            .line(132, 68, 218, 110, Color.BLACK, Width.W2, LineStyle.DOTTED)
            .rectangle(238, 68, 316, 110, Color.RED, Width.W2, FillMode.EMPTY)
            .rectangle(328, 78, 378, 102, Color.BLACK, Width.W1, FillMode.FULL)
            .circle(420, 89, 22, Color.YELLOW, Width.W2, FillMode.FULL)
            .circle(474, 89, 22, Color.RED, Width.W2, FillMode.EMPTY)
            .pieSlice(530, 89, 24, 0, 120, Color.RED)
            .pieSlice(530, 89, 24, 120, 120, Color.YELLOW)
            .pieSlice(530, 89, 24, 240, 120, Color.BLACK)
            .image(584, 66, image, Color.TRANSPARENT)
            .text(TextBox.builder(24, 132, 350, 112)
                .background(Color.YELLOW)
                .horizontalAlign(HorizontalAlign.CENTER)
                .verticalAlign(VerticalAlign.MIDDLE)
                .lineSpacing(4)
                .spans(List.of(
                    span("Temperature\n", 16, Color.BLACK, FontWeight.REGULAR),
                    span("21 ", 42, Color.RED, FontWeight.BOLD),
                    span("°C", 26, Color.RED, FontWeight.REGULAR)
                )).build())
            .text(TextBox.builder(398, 132, 370, 112)
                .background(Color.WHITE)
                .verticalAlign(VerticalAlign.MIDDLE)
                .lineSpacing(3)
                .overflow(TextOverflow.CLIP)
                .spans(List.of(
                    TextSpan.builder("MESSAGE")
                        .family(DEVICE_FONT).weight(FontWeight.BOLD).size(18)
                        .underline(true).letterSpacing(2).build(),
                    span("\nRødgrød med fløde · ÆØÅ æøå\n", 17,
                        Color.BLACK, FontWeight.REGULAR),
                    TextSpan.builder("14:30").family(DEVICE_FONT).size(17)
                        .strikeout(true).build(),
                    span("  Updated to 15:00", 17, Color.RED, FontWeight.REGULAR)
                )).build())
            .text(TextBox.builder(24, 248, 350, 20)
                .horizontalAlign(HorizontalAlign.RIGHT)
                .verticalAlign(VerticalAlign.BOTTOM)
                .overflow(TextOverflow.CLIP)
                .span(span("Native text · right / bottom aligned", 12, Color.BLACK, FontWeight.REGULAR))
                .build());
    }

    private static TextSpan span(String text, int size, Color color, FontWeight weight) {
        return TextSpan.builder(text)
            .family(DEVICE_FONT)
            .weight(weight)
            .size(size)
            .color(color)
            .build();
    }
}
