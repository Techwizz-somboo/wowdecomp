#ifndef HOT_MSDF_VALIDATOR_H
#define HOT_MSDF_VALIDATOR_H

namespace msdfgen {
class FontHandle;
}

namespace HoT::MSDF::Validator {

// False when a printable ASCII glyph has a self intersecting contour after geometry resolve, those break distance fields.
bool IsFontCompatible(msdfgen::FontHandle* font);

}

#endif
