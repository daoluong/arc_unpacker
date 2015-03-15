// PGX image
//
// Company:   Rune
// Engine:    GLib
// Extension: -
// Archives:  GLib2

#include "formats/image.h"
#include "formats/glib/gml_decoder.h"
#include "formats/glib/pgx_converter.h"
using namespace Formats::Glib;

namespace
{
    const std::string magic("PGX\x00", 4);
}

void PgxConverter::decode_internal(File &file) const
{
    if (file.io.read(magic.size()) != magic)
        throw std::runtime_error("Not a PGX image");

    file.io.skip(4);
    size_t image_width = file.io.read_u32_le();
    size_t image_height = file.io.read_u32_le();
    bool transparent = file.io.read_u16_le();
    file.io.skip(2);
    size_t source_size = file.io.read_u32_le();
    size_t target_size = image_width * image_height * 4;

    BufferedIO source_io;
    BufferedIO target_io;
    file.io.seek(file.io.size() - source_size);
    source_io.write_from_io(file.io, source_size);
    source_io.seek(0);
    target_io.reserve(target_size);

    GmlDecoder::decode(source_io, target_io);

    if (!transparent)
    {
        uint8_t *buffer = reinterpret_cast<uint8_t*>(target_io.buffer());
        uint8_t *buffer_guardian = buffer + target_io.size();
        buffer += 3;
        while (buffer < buffer_guardian)
        {
            *buffer = 0xff;
            buffer += 4;
        }
    }

    target_io.seek(0);

    std::unique_ptr<Image> image = Image::from_pixels(
        image_width,
        image_height,
        target_io.read_until_end(),
        IMAGE_PIXEL_FORMAT_BGRA);
    image->update_file(file);
}
