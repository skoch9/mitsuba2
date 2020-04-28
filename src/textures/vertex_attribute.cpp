#include <mitsuba/render/mesh.h>
#include <mitsuba/render/mesh_attribute.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/srgb.h>

NAMESPACE_BEGIN(mitsuba)

/**!
TODO: add description
 */

// Forward declaration of specialized bitmap texture
template <typename Float, typename Spectrum, uint32_t Size>
class VertexAttributeImpl;

template <typename Float, typename Spectrum>
class VertexAttribute final : public MeshAttribute<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(MeshAttribute, m_attribute_name, m_attribute_size, m_scale);
    MTS_IMPORT_TYPES(MeshAttribute)

    VertexAttribute(const Properties &props) : MeshAttribute(props) {
    }
    
    template <uint32_t Size>
    using Impl = VertexAttributeImpl<Float, Spectrum, Size>;

    /**
     * Recursively expand into an implementation specialized to the
     * actual loaded image.
     */
    std::vector<ref<Object>> expand() const override {
        if (m_impl)
            return { m_impl };

        Properties props;
        props.set_id(this->id());
        props.set_string("attribute_name", m_attribute_name);
        props.set_long("attribute_size", m_attribute_size);
        props.set_float("scale", m_scale);

        switch (m_attribute_size) {
            case 1: m_impl = (Object *) new Impl<1>(props); break;
            case 3: m_impl = (Object *) new Impl<3>(props); break;

            default:
                Log(Error, "Unsupported attribute size count: %d (expected 1 or 3)", m_attribute_size);
                Throw("Unsupported attribute size count: %d (expected 1 or 3)", m_attribute_size);
        }

        return { m_impl };
    }

    virtual void init(const Mesh<Float, Spectrum> */*parent_mesh*/, const DynamicBuffer<Float> */*data*/) override {
        NotImplementedError("init");
    }

    MTS_DECLARE_CLASS()

protected:
    const Mesh<Float, Spectrum> *m_mesh;
    const DynamicBuffer<Float> *m_vertex_attribute_buf;

    // TODO: check if this is a valid solution
    mutable ref<Object> m_impl;
};

template <typename Float, typename Spectrum, uint32_t Size>
class VertexAttributeImpl final : public MeshAttribute<Float, Spectrum> {
public:
    MTS_IMPORT_BASE(MeshAttribute, m_attribute_name, m_attribute_size, m_scale);
    MTS_IMPORT_TYPES(MeshAttribute)

    VertexAttributeImpl(const Properties &props)
    : MeshAttribute(props) {}

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

        // if constexpr (Size == 1) {
        //     return si.shape->eval_attribute_1(m_attribute_name, si, active);
        // } else {
        //     return si.shape->eval_attribute_3(m_attribute_name, si, active);
        // }

        auto fi = m_mesh->face_indices(si.prim_index, active);
        auto[b0, b1, b2] = m_mesh->barycentric_coordinates(si, active);

        using StorageType = std::conditional_t<Size == 1, Float, Color3f>;

        StorageType v0 = gather<StorageType>(*m_vertex_attribute_buf, fi[0], active),
                    v1 = gather<StorageType>(*m_vertex_attribute_buf, fi[1], active),
                    v2 = gather<StorageType>(*m_vertex_attribute_buf, fi[2], active);

        // Barycentric interpolation
        if constexpr (is_spectral_v<Spectrum> && Size == 3) {
            // Evaluate spectral upsampling model from stored coefficients
            UnpolarizedSpectrum c0, c1, c2;

            c0 = srgb_model_eval<UnpolarizedSpectrum>(v0, si.wavelengths);
            c1 = srgb_model_eval<UnpolarizedSpectrum>(v1, si.wavelengths);
            c2 = srgb_model_eval<UnpolarizedSpectrum>(v2, si.wavelengths);

            return fmadd(c0, b0, fmadd(c1, b1, c2 * b2)) * 0.00390625f;
        } else {
            return fmadd(v0, b0, fmadd(v1, b1, v2 * b2)) * 0.00390625f;
        }
    }

    // void traverse(TraversalCallback *callback) override {
    //     callback->put_parameter("transform", m_transform);
    //     callback->put_object("color0", m_color0.get());
    //     callback->put_object("color1", m_color1.get());
    // }

    virtual void init(const Mesh<Float, Spectrum> *parent_mesh, const DynamicBuffer<Float> *data) override {
        m_mesh = parent_mesh;
        m_vertex_attribute_buf = data;
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "VertexAttributeImpl[" << std::endl
            << "  name = \"" << m_attribute_name << "\"," << std::endl
            << "  size = \"" << m_attribute_size << "\"" << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
protected:
    const Mesh<Float, Spectrum> *m_mesh;
    const DynamicBuffer<Float> *m_vertex_attribute_buf;
};

MTS_IMPLEMENT_CLASS_VARIANT(VertexAttribute, MeshAttribute)
MTS_EXPORT_PLUGIN(VertexAttribute, "Vertex attribute")

NAMESPACE_BEGIN(detail)
template <uint32_t Size>
constexpr const char * vertex_attribute_class_name() {
    if constexpr (Size == 1)
        return "VertexAttributeImpl_1";
    else
        return "VertexAttributeImpl_3";
}
NAMESPACE_END(detail)

template <typename Float, typename Spectrum, uint32_t Size>
Class *VertexAttributeImpl<Float, Spectrum, Size>::m_class
    = new Class(detail::vertex_attribute_class_name<Size>(), "Texture",
                ::mitsuba::detail::get_variant<Float, Spectrum>(), nullptr, nullptr);

template <typename Float, typename Spectrum, uint32_t Size>
const Class* VertexAttributeImpl<Float, Spectrum, Size>::class_() const {
    return m_class;
}

NAMESPACE_END(mitsuba)
