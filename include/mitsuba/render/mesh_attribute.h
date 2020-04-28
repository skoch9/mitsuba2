#include <mitsuba/render/texture.h>
#include <mitsuba/core/properties.h>

NAMESPACE_BEGIN(mitsuba)

/**!

TODO: description

 */

template <typename Float, typename Spectrum>
class MTS_EXPORT_RENDER MeshAttribute : public Texture<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES(Texture)
    MTS_DECLARE_CLASS()

    virtual void init(const Mesh<Float, Spectrum> *parent_mesh, const DynamicBuffer<Float> *data) = 0;

    const std::string& name() const { return m_attribute_name; }
    size_t size() const { return m_attribute_size; }

protected:
    MeshAttribute(const Properties &);
    virtual ~MeshAttribute();

protected:
    std::string m_attribute_name;
    size_t m_attribute_size;
    float m_scale;
};

MTS_EXTERN_CLASS_RENDER(MeshAttribute)
NAMESPACE_END(mitsuba)
