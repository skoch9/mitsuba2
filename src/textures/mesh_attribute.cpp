#include <mitsuba/render/texture.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/srgb.h>

NAMESPACE_BEGIN(mitsuba)

/**!
TODO: add description
 */

// Forward declaration of specialized bitmap texture
template <typename Float, typename Spectrum, uint32_t Size, bool Vertex>
class MeshAttributeImpl;

template <typename Float, typename Spectrum>
class MeshAttribute final : public Texture<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES(Texture)

    MeshAttribute(const Properties &props)
    : Texture(props) {
        m_name = props.string("name");
        m_size = props.size_("size");

        m_scale = props.float_("scale", 1.f);

        m_type = props.string("type", "vertex");
        if (m_type != "vertex" && m_type != "face")
            Throw("Invalid mesh attribute type: must be either \"vertex\" or \"face\" but was \"%s\".", m_type.c_str());
    }

    const std::string& name() const { return m_name; }
    size_t size() const { return m_size; }
    
    template <uint32_t Size, bool Vertex>
    using Impl = MeshAttributeImpl<Float, Spectrum, Size, Vertex>;

    /**
     * Recursively expand into an implementation specialized to the
     * actual loaded image.
     */
    std::vector<ref<Object>> expand() const override {
        ref<Object> result;

        Properties props;
        props.set_id(this->id());
        props.set_string("name", m_name);
        props.set_float("scale", m_scale);
        
        if (m_type == "vertex") {
            switch (m_size) {
                case 1: result = (Object *) new Impl<1, true>(props); break;
                case 3: result = (Object *) new Impl<3, true>(props); break;

                default:
                    Log(Error, "Unsupported attribute size count: %d (expected 1 or 3)", m_size);
                    Throw("Unsupported attribute size count: %d (expected 1 or 3)", m_size);
            }
        } else {
            switch (m_size) {
                case 1: result = (Object *) new Impl<1, false>(props); break;
                case 3: result = (Object *) new Impl<3, false>(props); break;

                default:
                    Log(Error, "Unsupported attribute size count: %d (expected 1 or 3)", m_size);
                    Throw("Unsupported attribute size count: %d (expected 1 or 3)", m_size);
            }
        }

        return { result };
    }

    MTS_DECLARE_CLASS()
protected:
    std::string m_name;
    std::string m_type;
    size_t m_size;
    float m_scale;
};

template <typename Float, typename Spectrum, uint32_t Size, bool Vertex>
class MeshAttributeImpl final : public Texture<Float, Spectrum> {
public:
    MTS_IMPORT_TYPES(Texture)

    MeshAttributeImpl(const Properties &props)
    : Texture(props) {
        m_name = props.string("name");
        m_scale = props.float_("scale");
    }

    UnpolarizedSpectrum eval(const SurfaceInteraction3f &si, Mask active) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        auto result = interpolate(si, active);

        if constexpr (Size == 3 && is_monochromatic_v<Spectrum>)
            return luminance(result);
        else
            return result;
    }
    
    Float eval_1(const SurfaceInteraction3f &si, Mask active = true) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if constexpr (Size == 3 && is_spectral_v<Spectrum>) {
            ENOKI_MARK_USED(si);
            Throw("eval_1(): The vertex attribute %s was queried for a scalar value, but "
                  "conversion into spectra was requested!",
                  to_string());
        } else {
            auto result = interpolate(si, active);

            if constexpr (Size == 3)
                return luminance(result);
            else
                return result;
        }
    }
    
    Color3f eval_3(const SurfaceInteraction3f &si, Mask active = true) const override {
        MTS_MASKED_FUNCTION(ProfilerPhase::TextureEvaluate, active);

        if constexpr (Size != 3) {
            ENOKI_MARK_USED(si);
            Throw("eval_3(): The vertex attribute %s was queried for a RGB value, but it is "
                  "monochromatic!", to_string());
        } else {
            return interpolate(si, active);
        }
    }

    MTS_INLINE auto interpolate(const SurfaceInteraction3f &si, Mask active) const {
        if constexpr (!is_array_v<Mask>)
            active = true;

        if constexpr (Vertex) {
            if constexpr (Size == 1) {
                return si.shape->eval_vertex_attribute_1(m_name, si, active) * m_scale;
            } else {
                return si.shape->eval_vertex_attribute_3(m_name, si, active) * m_scale;
            }
        } else {
            if constexpr (Size == 1) {
                return si.shape->eval_face_attribute_1(m_name, si, active) * m_scale;
            } else {
                return si.shape->eval_face_attribute_3(m_name, si, active) * m_scale;
            }
        }
    }

    // void traverse(TraversalCallback *callback) override {
    //     callback->put_parameter("transform", m_transform);
    //     callback->put_object("color0", m_color0.get());
    //     callback->put_object("color1", m_color1.get());
    // }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "MeshAttributeImpl[" << std::endl
            << "  name = \"" << m_name << "\"," << std::endl
            << "  type = \"" << (Vertex ? "vertex" : "face") << "\"," << std::endl
            << "  size = \"" << Size << "\"," << std::endl
            << "  scale = \"" << m_scale << "\"" << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
protected:
    std::string m_name;
    float m_scale;
};

MTS_IMPLEMENT_CLASS_VARIANT(MeshAttribute, Texture)
MTS_EXPORT_PLUGIN(MeshAttribute, "Mesh attribute")

NAMESPACE_BEGIN(detail)
template <uint32_t Size, bool Vertex>
constexpr const char * mesh_attribute_class_name() {
    if constexpr (Vertex)
        if constexpr (Size == 1)
            return "MeshAttributeImpl_Vertex_1";
        else
            return "MeshAttributeImpl_Vertex_3";
    else
        if constexpr (Size == 1)
            return "MeshAttributeImpl_Face_1";
        else
            return "MeshAttributeImpl_Face_3";
}
NAMESPACE_END(detail)

template <typename Float, typename Spectrum, uint32_t Size, bool Vertex>
Class *MeshAttributeImpl<Float, Spectrum, Size, Vertex>::m_class
    = new Class(detail::mesh_attribute_class_name<Size, Vertex>(), "Texture",
                ::mitsuba::detail::get_variant<Float, Spectrum>(), nullptr, nullptr);

template <typename Float, typename Spectrum, uint32_t Size, bool Vertex>
const Class* MeshAttributeImpl<Float, Spectrum, Size, Vertex>::class_() const {
    return m_class;
}

NAMESPACE_END(mitsuba)
