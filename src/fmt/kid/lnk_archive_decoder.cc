#include "fmt/kid/lnk_archive_decoder.h"
#include "err.h"
#include "fmt/kid/cps_file_decoder.h"
#include "fmt/kid/decompressor.h"
#include "fmt/kid/prt_image_decoder.h"
#include "fmt/kid/waf_audio_decoder.h"
#include "io/buffered_io.h"
#include "util/range.h"

using namespace au;
using namespace au::fmt::kid;

static const bstr magic = "LNK\x00"_b;
static const bstr compress_magic = "lnd\x00"_b;

namespace
{
    struct TableEntry final
    {
        std::string name;
        bool compressed;
        u32 offset;
        u32 size;
    };

    using Table = std::vector<std::unique_ptr<TableEntry>>;
}

static bstr lnd_decompress(const bstr &input)
{
    io::BufferedIO input_io(input);
    if (input_io.read(compress_magic.size()) != compress_magic)
        throw err::CorruptDataError("Unexpected file header");
    input_io.skip(4);
    size_t size_original = input_io.read_u32_le();
    input_io.skip(4);
    return decompress(input_io.read_to_eof(), size_original);
}

static Table read_table(io::IO &arc_io)
{
    Table table;
    u32 file_count = arc_io.read_u32_le();
    arc_io.skip(8);
    auto file_data_start = arc_io.tell() + (file_count << 5);
    for (auto i : util::range(file_count))
    {
        std::unique_ptr<TableEntry> entry(new TableEntry);
        entry->offset = arc_io.read_u32_le() + file_data_start;
        u32 tmp = arc_io.read_u32_le();
        entry->compressed = tmp & 1;
        entry->size = tmp >> 1;
        entry->name = arc_io.read_to_zero(24).str();
        table.push_back(std::move(entry));
    }
    return table;
}

static std::unique_ptr<File> read_file(io::IO &arc_io, const TableEntry &entry)
{
    std::unique_ptr<File> file(new File);
    file->name = entry.name;

    arc_io.seek(entry.offset);
    bstr data = arc_io.read(entry.size);

    int key_pos = -1;
    if (file->has_extension(".wav"))
        key_pos = 0;
    else if (file->has_extension(".jpg"))
        key_pos = 0x1100;
    else if (file->has_extension(".scr"))
        key_pos = 0x1000;

    if (key_pos >= 0 && key_pos < static_cast<int>(entry.size))
    {
        u8 key = 0;
        for (u8 c : entry.name)
            key += c;

        for (size_t i = 0; i < 0x100 && key_pos + i < entry.size; i++)
        {
            data.get<u8>()[key_pos + i] -= key;
            key = key * 0x6D - 0x25;
        }
    }

    if (entry.compressed)
        file->io.write(lnd_decompress(data));
    else
        file->io.write(data);

    return file;
}

struct LnkArchiveDecoder::Priv final
{
    CpsFileDecoder cps_file_decoder;
    PrtImageDecoder prt_image_decoder;
    WafAudioDecoder waf_audio_decoder;
};

LnkArchiveDecoder::LnkArchiveDecoder() : p(new Priv)
{
    add_decoder(&p->cps_file_decoder);
    add_decoder(&p->prt_image_decoder);
    add_decoder(&p->waf_audio_decoder);
}

LnkArchiveDecoder::~LnkArchiveDecoder()
{
}

bool LnkArchiveDecoder::is_recognized_internal(File &arc_file) const
{
    return arc_file.io.read(magic.size()) == magic;
}

void LnkArchiveDecoder::unpack_internal(File &arc_file, FileSaver &saver) const
{
    arc_file.io.skip(magic.size());
    Table table = read_table(arc_file.io);
    for (auto &entry : table)
        saver.save(read_file(arc_file.io, *entry));
}

static auto dummy = fmt::Registry::add<LnkArchiveDecoder>("kid/lnk");