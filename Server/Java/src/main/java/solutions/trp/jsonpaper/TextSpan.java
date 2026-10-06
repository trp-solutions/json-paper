package solutions.trp.jsonpaper;

import com.fasterxml.jackson.annotation.JsonProperty;
import com.fasterxml.jackson.annotation.JsonPropertyOrder;
import java.util.Objects;

@JsonPropertyOrder({"text", "family", "weight", "size", "color", "letter_spacing", "underline", "strikeout"})
public record TextSpan(
    String text,
    String family,
    FontWeight weight,
    int size,
    Color color,
    @JsonProperty("letter_spacing") int letterSpacing,
    boolean underline,
    boolean strikeout
) {
    public static final int MIN_SIZE = 6;
    public static final int MAX_SIZE = 96;

    public TextSpan {
        Objects.requireNonNull(text, "text");
        family = Objects.requireNonNull(family, "family");
        if (family.isBlank()) throw new IllegalArgumentException("family must not be blank");
        Objects.requireNonNull(weight, "weight");
        Objects.requireNonNull(color, "color");
        if (color == Color.TRANSPARENT) throw new IllegalArgumentException("text color cannot be transparent");
        if (size < MIN_SIZE || size > MAX_SIZE) throw new IllegalArgumentException("size must be 6-96 pixels");
    }

    public TextSpan(String text, int size, Color color) {
        this(text, "helvetica", FontWeight.REGULAR, size, color, 0, false, false);
    }

    public static Builder builder(String text) { return new Builder(text); }

    public static final class Builder {
        private final String text;
        private String family = "helvetica";
        private FontWeight weight = FontWeight.REGULAR;
        private int size = 16;
        private Color color = Color.BLACK;
        private int letterSpacing;
        private boolean underline;
        private boolean strikeout;
        private Builder(String text) { this.text = text; }
        public Builder family(String v) { family = v; return this; }
        public Builder weight(FontWeight v) { weight = v; return this; }
        public Builder size(int v) { size = v; return this; }
        public Builder color(Color v) { color = v; return this; }
        public Builder letterSpacing(int v) { letterSpacing = v; return this; }
        public Builder underline(boolean v) { underline = v; return this; }
        public Builder strikeout(boolean v) { strikeout = v; return this; }
        public TextSpan build() { return new TextSpan(text, family, weight, size, color, letterSpacing, underline, strikeout); }
    }
}
