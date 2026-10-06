package solutions.trp.jsonpaper;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.junit.jupiter.api.Test;

import java.util.List;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.junit.jupiter.api.Assertions.assertThrows;

class DisplayTest {
    private final ObjectMapper objectMapper = new ObjectMapper();

    @Test
    void writesTransformsAndPixelsWithFirmwareFieldNames() throws Exception {
        JsonNode root = objectMapper.readTree(new Display()
            .setRotate(90).setMirroring(3).setPixel(12, 13, Color.RED).toJson());
        assertEquals("set_rotate", root.at("/commands/0/cmd").textValue());
        assertEquals(90, root.at("/commands/0/args/rotate").intValue());
        assertEquals("set_mirroring", root.at("/commands/1/cmd").textValue());
        assertEquals(3, root.at("/commands/1/args/mirror").intValue());
        assertEquals("set_pixel", root.at("/commands/2/cmd").textValue());
        assertEquals(12, root.at("/commands/2/args/x").intValue());
        assertEquals(13, root.at("/commands/2/args/y").intValue());
        assertEquals("red", root.at("/commands/2/args/color").textValue());
        assertThrows(IllegalArgumentException.class, () -> new Display().setRotate(45));
        assertThrows(IllegalArgumentException.class, () -> new Display().setMirroring(4));
        assertThrows(IllegalArgumentException.class,
            () -> new Display().setPixel(0, 0, Color.TRANSPARENT));
    }

    @Test
    void richTextSampleCoversFirmwareCommandsAndRasterFooter() throws Exception {
        DisplayDocument sample = RichTextSample.create(
            new java.awt.image.BufferedImage(120, 36, java.awt.image.BufferedImage.TYPE_INT_ARGB),
            new RasterFontFamily(
                java.nio.file.Path.of("../../Arduino/src/font/assets/Helvetica-Regular.ttf"),
                java.nio.file.Path.of("../../Arduino/src/font/assets/Helvetica-Bold.ttf")));
        assertEquals(java.util.Set.of("clear", "clear_window", "set_rotate", "set_mirroring",
            "set_pixel", "draw_point", "draw_line", "draw_rectangle", "draw_circle",
            "draw_pie_slice", "draw_image", "draw_text"), sample.commands().stream()
                .map(DisplayCommand::cmd).collect(java.util.stream.Collectors.toSet()));
        byte[] json = objectMapper.writeValueAsBytes(sample);
        assertTrue(json.length <= Display.MAX_DOCUMENT_BYTES);
        DisplayCommand footer = sample.commands().get(sample.commands().size() - 1);
        assertEquals("draw_image", footer.cmd());
        assertEquals(370 * 20 / 2, java.util.Base64.getDecoder()
            .decode((String) footer.args().get("data")).length);
        assertEquals(19, RichTextSample.create().commands().size());
        assertEquals("helvetica", new TextSpan("default", 16, Color.BLACK).family());
    }

    @Test
    void writesEveryPhpCommandWithFirmwareFieldNames() throws Exception {
        Display display = new Display()
            .clear(Color.WHITE)
            .clearWindow(1, 2, 3, 4, Color.RED)
            .point(5, 6, Color.YELLOW, Width.W2, PointStyle.RIGHTUP)
            .line(7, 8, 9, 10, Color.BLACK, Width.W3, LineStyle.DOTTED)
            .rectangle(11, 12, 13, 14, Color.RED, Width.W4, FillMode.FULL)
            .circle(15, 16, 17, Color.YELLOW, Width.W2, FillMode.EMPTY)
            .pieSlice(18, 19, 20, 270, 90, Color.RED)
            .text(TextBox.builder(21, 22, 200, 40)
                .span(TextSpan.builder("A \"quoted\" value").size(24).build())
                .build());

        JsonNode root = objectMapper.readTree(display.toJson());

        assertEquals("2.0", root.get("version").textValue());
        assertEquals(8, root.get("commands").size());
        assertEquals("clear_window", root.at("/commands/1/cmd").textValue());
        assertEquals(1, root.at("/commands/1/args/x_start").intValue());
        assertEquals("rightup", root.at("/commands/2/args/style").textValue());
        assertEquals("dotted", root.at("/commands/3/args/style").textValue());
        assertEquals("full", root.at("/commands/4/args/fill").textValue());
        assertEquals("draw_pie_slice", root.at("/commands/6/cmd").textValue());
        assertEquals(270, root.at("/commands/6/args/start_angle").intValue());
        assertEquals(90, root.at("/commands/6/args/sweep_angle").intValue());
        assertEquals(
            "A \"quoted\" value",
            root.at("/commands/7/args/spans/0/text").textValue()
        );
        assertEquals("helvetica", root.at("/commands/7/args/spans/0/family").textValue());
        assertEquals(
            "transparent",
            root.at("/commands/7/args/background").textValue()
        );
    }

    @Test
    void appliesPhpDefaults() {
        DisplayDocument document = new Display()
            .point(1, 2)
            .line(1, 2, 3, 4)
            .rectangle(1, 2, 3, 4)
            .circle(1, 2, 1)
            .text(new TextBox(1, 2, 100, 20, List.of(new TextSpan("text", 16, Color.BLACK))))
            .document();

        List<DisplayCommand> commands = document.commands();
        assertEquals("black", commands.get(0).args().get("color"));
        assertEquals("1x1", commands.get(0).args().get("width"));
        assertEquals("around", commands.get(0).args().get("style"));
        assertEquals("solid", commands.get(1).args().get("style"));
        assertEquals("empty", commands.get(2).args().get("fill"));
        assertEquals("empty", commands.get(3).args().get("fill"));
        assertEquals("word", commands.get(4).args().get("wrap"));
        assertEquals("transparent", commands.get(4).args().get("background"));
    }

    @Test
    void documentIsAnImmutableSnapshot() throws Exception {
        Display display = new Display().clear(Color.WHITE);
        DisplayDocument snapshot = display.document();

        display.point(1, 1);

        assertEquals(1, snapshot.commands().size());
        assertEquals(2, display.document().commands().size());
        assertFalse(snapshot.commands().isEmpty());
        assertTrue(snapshot.commands().get(0).args().containsKey("color"));
        assertEquals(
            objectMapper.readTree(objectMapper.writeValueAsString(snapshot)),
            objectMapper.readTree(new ObjectMapper().writeValueAsString(snapshot))
        );
    }

    @Test
    void prettyJsonRepresentsTheSameDocument() throws Exception {
        Display display = new Display().text(
            new TextBox(1, 2, 100, 20, List.of(new TextSpan("hello", 16, Color.BLACK))));

        assertEquals(
            objectMapper.readTree(display.toJson()),
            objectMapper.readTree(display.toPrettyJson())
        );
        assertTrue(display.toPrettyJson().contains(System.lineSeparator()));
    }
}
