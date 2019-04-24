#include "map.hpp"

#define FDP_MODULE "map"
#include "log.hpp"
#include "utils/fnview.hpp"

#include <fstream>
#include <sstream>

struct sym::Map::Data
{
    // members
    fs::path      filename;
    std::ifstream filestream;
    bool          settedup;
};

sym::Map::Map()
    : d_(std::make_unique<Data>())
{
    d_->settedup = false;
}

sym::Map::~Map()
{
}

bool sym::Map::setup()
{
    auto& d         = *d_;
    const auto path = getenv("_LINUX_SYMBOL_PATH");
    if(!path)
        return FAIL(false, "unable to find _LINUX_SYMBOL_PATH");

    d.filename = fs::path(fs::path(path) / "System.map");

    d.filestream = std::ifstream(d.filename.generic_string().data());
    if(!d.filestream)
        return FAIL(false, "unable to open {}", d.filename.generic_string());

    std::string    row;
    std::string    str_offset;
    sym::ModCursor cursor;
    char           type;
    while(std::getline(d.filestream, row))
    {
        if(!(std::istringstream(row) >> str_offset >> type >> cursor.symbol))
            return FAIL(false, "unable to parse row '{}' in file {}", row, d.filename.generic_string());

        std::istringstream iss_offset(str_offset);
        iss_offset >> std::hex;
        if(!(iss_offset >> cursor.offset))
            return FAIL(false, "unable to parse hex '{}' in file {}", str_offset, d.filename.generic_string());
    }

    d.settedup = true;
    return true;
}

span_t sym::Map::span()
{
    return {};
}

opt<uint64_t> sym::Map::symbol(const std::string& symbol)
{
    opt<uint64_t> symbol_offset = {};

    sym_list([&](std::string name, uint64_t offset)
    {
        if(name == symbol)
        {
            symbol_offset = offset;
            return WALK_STOP;
        }
        return WALK_NEXT;
    });

    return symbol_offset;
}

bool sym::Map::sym_list(sym::on_sym_fn on_sym)
{
    auto& d = *d_;
    if(!d.settedup)
        return FAIL(false, "map parser has not been set up");

    std::string str_offset;
    uint64_t    offset;
    char        type;
    std::string name;

    d.filestream.clear();
    d.filestream.seekg(0, std::ios::beg);
    while(d.filestream >> str_offset >> type >> name)
    {
        std::istringstream(str_offset) >> std::hex >> offset;

        const auto err = on_sym(name, offset);
        if(err == WALK_STOP)
            return true;
    }

    return false;
}

opt<uint64_t> sym::Map::struc_offset(const std::string& /*struc*/, const std::string& /*member*/)
{
    return {};
}

opt<size_t> sym::Map::struc_size(const std::string& /*struc*/)
{
    return {};
}

opt<sym::ModCursor> sym::Map::symbol(uint64_t addr)
{
    opt<sym::ModCursor> cursor = {};

    sym_list([&](std::string name, uint64_t offset)
    {
        if(offset == addr)
        {
            cursor         = sym::ModCursor();
            cursor->symbol = name;
            cursor->offset = offset;
            return WALK_STOP;
        }
        return WALK_NEXT;
    });

    return cursor;
}