#include <mitsuba/core/plugin.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/mesh_attribute.h>

NAMESPACE_BEGIN(mitsuba)

// =======================================================================
//! @{ \name MeshAttribute implementations
// =======================================================================

MTS_VARIANT MeshAttribute<Float, Spectrum>::MeshAttribute(const Properties &props)
    : Texture(props) {
    m_attribute_name = props.string("attribute_name");
    m_attribute_size = props.size_("attribute_size");

    m_scale = props.float_("scale", 1.f);
}

MTS_VARIANT MeshAttribute<Float, Spectrum>::~MeshAttribute() { }

//! @}
// =======================================================================

MTS_IMPLEMENT_CLASS_VARIANT(MeshAttribute, Texture, "MeshAttribute")

MTS_INSTANTIATE_CLASS(MeshAttribute)
NAMESPACE_END(mitsuba)
