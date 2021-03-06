#include <osg/Notify>
#include <osg/Geode>
#include <osg/Geometry>

#include <osgDB/FileNameUtils>
#include <osgDB/FileUtils>
#include <osgDB/fstream>
#include <osgDB/Registry>

#include <iostream>
#include <stdio.h>
#include <string.h>

#include "GeometryUtil.h"

#define private public   //for indices_map_
#include "compression/encode.h"
#include "core/cycle_timer.h"
#include "io/mesh_io.h"
#include "io/point_cloud_io.h"
#undef private


struct DracoOptions {
    DracoOptions();

    bool is_point_cloud;
    int pos_quantization_bits;
    int tex_coords_quantization_bits;
    int normals_quantization_bits;
    int compression_level;
    std::string input;
    std::string output;
};

DracoOptions::DracoOptions()
    : is_point_cloud(false),
    pos_quantization_bits(14),
    tex_coords_quantization_bits(12),
    normals_quantization_bits(10),
    compression_level(0) {}


void PrintOptions(const draco::PointCloud &pc, const DracoOptions &options) {
    printf("Encoder options:\n");
    printf("  Compression level = %d\n", options.compression_level);
    if (options.pos_quantization_bits <= 0) {
        printf("  Positions: No quantization\n");
    }
    else {
        printf("  Positions: Quantization = %d bits\n",
            options.pos_quantization_bits);
    }

    if (pc.GetNamedAttributeId(draco::GeometryAttribute::TEX_COORD) >= 0) {
        if (options.tex_coords_quantization_bits <= 0) {
            printf("  Texture coordinates: No quantization\n");
        }
        else {
            printf("  Texture coordinates: Quantization = %d bits\n",
                options.tex_coords_quantization_bits);
        }
    }

    if (pc.GetNamedAttributeId(draco::GeometryAttribute::NORMAL) >= 0) {
        if (options.normals_quantization_bits <= 0) {
            printf("  Normals: No quantization\n");
        }
        else {
            printf("  Normals: Quantization = %d bits\n",
                options.normals_quantization_bits);
        }
    }
    printf("\n");
}

int EncodePointCloudToFile(const draco::PointCloud &pc,
    const draco::EncoderOptions &options,
    const std::string &file) {
    draco::CycleTimer timer;
    // Encode the geometry.
    draco::EncoderBuffer buffer;
    timer.Start();
    if (!draco::EncodePointCloudToBuffer(pc, options, &buffer)) {
        printf("Failed to encode the point cloud.\n");
        return -1;
    }
    timer.Stop();
    // Save the encoded geometry into a file.
    std::ofstream out_file(file, std::ios::binary);
    if (!out_file) {
        printf("Failed to create the output file.\n");
        return -1;
    }
    out_file.write(buffer.data(), buffer.size());
    printf("Encoded point cloud saved to %s (%" PRId64 " ms to encode)\n",
        file.c_str(), timer.GetInMs());
    printf("\nEncoded size = %zu bytes\n\n", buffer.size());
    return 0;
}

int EncodeMeshToFile(const draco::Mesh &mesh,
    const draco::EncoderOptions &options,
    const std::string &file) {
    draco::CycleTimer timer;
    // Encode the geometry.
    draco::EncoderBuffer buffer;
    timer.Start();
    if (!draco::EncodeMeshToBuffer(mesh, options, &buffer)) {
        printf("Failed to encode the mesh.\n");
        return -1;
    }
    timer.Stop();
    // Save the encoded geometry into a file.
    std::ofstream out_file(file, std::ios::binary);
    if (!out_file) {
        printf("Failed to create the output file.\n");
        return -1;
    }
    out_file.write(buffer.data(), buffer.size());
    printf("Encoded mesh saved to %s (%" PRId64 " ms to encode)\n", file.c_str(),
        timer.GetInMs());
    printf("\nEncoded size = %zu bytes\n\n", buffer.size());
    return 0;
}

struct DarocOptionsStruct
{
    bool isPointCloud;
};

DarocOptionsStruct parseOptions(const osgDB::ReaderWriter::Options* options)
{
    DarocOptionsStruct localOptions;
    localOptions.isPointCloud = false;

    if (options != NULL)
    {
        std::istringstream iss(options->getOptionString());
        std::string opt;
        while (iss >> opt)
        {
            if (opt == "draco_point_cloud")
            {
                localOptions.isPointCloud = true;
            }
        }
    }

    return localOptions;
}


//osg node to daroc data
void osgNodeToDarocAttribute(GeometryFlat* gf, draco::PointCloud* pc)
{
    //num
    size_t num_positions_ = gf->m_geomtry_data.raw_vertex->size();
    size_t num_tex_coords_ = gf->m_geomtry_data.raw_uv0->size();
    size_t num_normals_ = gf->m_geomtry_data.raw_normal->size();
    size_t num_obj_faces_ = num_positions_ / 3;

    // attribute id
    int pos_att_id_ = 0;
    int tex_att_id_ = 0;
    int norm_att_id_ = 0;

    // Add attributes if they are present in the input data.
    if (num_positions_ > 0)
    {
        draco::GeometryAttribute va;
        va.Init(draco::GeometryAttribute::POSITION, nullptr, 3, draco::DT_FLOAT32, false,
            sizeof(float) * 3, 0);
        pos_att_id_ = pc->AddAttribute(va, true, num_positions_);
    }
    if (num_tex_coords_ > 0)
    {
        draco::GeometryAttribute va;
        va.Init(draco::GeometryAttribute::TEX_COORD, nullptr, 2, draco::DT_FLOAT32, false,
            sizeof(float) * 2, 0);
        tex_att_id_ = pc->AddAttribute(va, true, num_tex_coords_);
    }
    if (num_normals_ > 0)
    {
        draco::GeometryAttribute va;
        va.Init(draco::GeometryAttribute::NORMAL, nullptr, 3, draco::DT_FLOAT32, false,
            sizeof(float) * 3, 0);
        norm_att_id_ = pc->AddAttribute(va, true, num_normals_);
    }

    //num_positions_ = 0;
    //num_tex_coords_ = 0;
    //num_normals_ = 0;
    //int num_positions_i = 0;
    //int num_tex_coords_i = 0;
    //int num_normals_i = 0;

    for (size_t i = 0; i < num_positions_; i++)
    {
        float v[3];
        v[0] = (*gf->m_geomtry_data.raw_vertex)[i].x();
        v[1] = (*gf->m_geomtry_data.raw_vertex)[i].y();
        v[2] = (*gf->m_geomtry_data.raw_vertex)[i].z();
        pc->attribute(pos_att_id_)->SetAttributeValue(draco::AttributeValueIndex(i), v);
        //pc->attribute(pos_att_id_)->SetPointMapEntry(draco::PointIndex(i),draco::AttributeValueIndex(i));
    }

    for (size_t i = 0; i < num_tex_coords_; i++)
    {
        float uv[2];
        uv[0] = (*gf->m_geomtry_data.raw_uv0)[i].x();
        uv[1] = (*gf->m_geomtry_data.raw_uv0)[i].y();
        pc->attribute(tex_att_id_)->SetAttributeValue(draco::AttributeValueIndex(i), uv);
        //pc->attribute(tex_att_id_)->SetPointMapEntry(draco::PointIndex(i), draco::AttributeValueIndex(i));
    }

    for (size_t i = 0; i < num_normals_; i++)
    {
        float n[3];
        n[0] = (*gf->m_geomtry_data.raw_normal)[i].x();
        n[1] = (*gf->m_geomtry_data.raw_normal)[i].y();
        n[2] = (*gf->m_geomtry_data.raw_normal)[i].z();
        pc->attribute(norm_att_id_)->SetAttributeValue(draco::AttributeValueIndex(i), n);
        //pc->attribute(norm_att_id_)->SetPointMapEntry(draco::PointIndex(i), draco::AttributeValueIndex(i));
    }
}

class ReaderWriterDRC
    : public osgDB::ReaderWriter
{
public:

    ReaderWriterDRC()
    {
        supportsExtension("drc", "Daroc format");

        supportsOption("draco_point_cloud", "save file as PointCloud");
    }

    virtual const char* className() const { return "Daroc reader/writer"; }

    virtual ReadResult readNode(const std::string& file, const osgDB::ReaderWriter::Options* options) const
    {
        std::string ext = osgDB::getLowerCaseFileExtension(file);
        if (!acceptsExtension(ext)) return ReadResult::FILE_NOT_HANDLED;

        std::string fileName = osgDB::findDataFile(file, options);
        if (fileName.empty()) return ReadResult::FILE_NOT_FOUND;

        OSG_INFO << "Reading file " << fileName << std::endl;

        osg::Group* ret = new osg::Group();

        // open input stream
        std::ifstream input_file(fileName, std::ios::binary);
        if (!input_file)
        {
            printf("Failed opening the input file.\n");
            return ReadResult::FILE_NOT_FOUND;
        }

        // Read the file stream into a buffer.
        std::streampos file_size = 0;
        input_file.seekg(0, std::ios::end);
        file_size = input_file.tellg() - file_size;
        input_file.seekg(0, std::ios::beg);
        std::vector<char> data(file_size);
        input_file.read(data.data(), file_size);
        if (data.empty())
        {
            printf("Empty input file.\n");
            return ReadResult::FILE_NOT_FOUND;
        }

        // Create a draco decoding buffer. Note that no data is copied in this step.
        draco::DecoderBuffer buffer;
        buffer.Init(data.data(), data.size());

        draco::CycleTimer timer;
        // Decode the input data into a geometry.
        std::unique_ptr<draco::PointCloud> pc;
        draco::Mesh *mesh = nullptr;
        const draco::EncodedGeometryType geom_type =
            draco::GetEncodedGeometryType(&buffer);
        if (geom_type == draco::TRIANGULAR_MESH) {
            timer.Start();
            std::unique_ptr<draco::Mesh> in_mesh = draco::DecodeMeshFromBuffer(&buffer);
            timer.Stop();
            if (in_mesh) {
                mesh = in_mesh.get();
                pc = std::move(in_mesh);
            }
        }
        else if (geom_type == draco::POINT_CLOUD) {
            // Failed to decode it as mesh, so let's try to decode it as a point cloud.
            timer.Start();
            pc = draco::DecodePointCloudFromBuffer(&buffer);
            timer.Stop();
        }

        if (pc == nullptr)
        {
            printf("Failed to decode the input file.\n");
            return ReadResult::FILE_NOT_FOUND;
        }



        //get index attribute 
        osg::ref_ptr<osg::Vec3Array> index_vertex = new osg::Vec3Array();
        osg::ref_ptr<osg::Vec3Array> index_normal = new osg::Vec3Array();
        osg::ref_ptr<osg::Vec4Array> index_color = new osg::Vec4Array();
        osg::ref_ptr<osg::Vec2Array> index_uv0 = new osg::Vec2Array();

        //vertex
        const draco::PointAttribute *const att_vertex =
            pc->GetNamedAttribute(draco::GeometryAttribute::POSITION);
        if (att_vertex && att_vertex->size() > 0)
        {
            std::array<float, 3> value;
            int size = std::max<int>(att_vertex->size(), att_vertex->indices_map_.size());
            for (draco::PointIndex i(0); i < size; ++i)
            {
                att_vertex->GetMappedValue(i, &value[0]);
                index_vertex->push_back(osg::Vec3(value[0], value[1], value[2]));
            }
        }

        //normal
        const draco::PointAttribute *const att_normal =
            pc->GetNamedAttribute(draco::GeometryAttribute::NORMAL);
        if (att_normal && att_normal->size() > 0)
        {
            std::array<float, 3> value;
            int size = std::max<int>(att_normal->size(), att_normal->indices_map_.size());
            for (draco::PointIndex i(0); i < size; ++i)
            {
                att_normal->GetMappedValue(i, &value[0]);
                index_normal->push_back(osg::Vec3(value[0], value[1], value[2]));
            }
        }

        //uv0
        const draco::PointAttribute *const att_uv0 =
            pc->GetNamedAttribute(draco::GeometryAttribute::TEX_COORD);
        if (att_uv0 && att_uv0->size() > 0)
        {
            std::array<float, 2> value;
            int size = std::max<int>(att_uv0->size(), att_uv0->indices_map_.size());
            for (draco::PointIndex i(0); i < size; ++i)
            {
                att_uv0->GetMappedValue(i, &value[0]);
                index_uv0->push_back(osg::Vec2(value[0], value[1]));
            }
        }

        //color
        const draco::PointAttribute *const att_color =
            pc->GetNamedAttribute(draco::GeometryAttribute::COLOR);
        if (att_color && att_color->size() > 0)
        {
            std::array<float, 4> value;
            int size = std::max<int>(att_color->size(), att_color->indices_map_.size());
            for (draco::PointIndex i(0); i < size; ++i)
            {
                att_color->GetMappedValue(i, &value[0]);
                index_color->push_back(osg::Vec4(value[0], value[1], value[2], value[3]));
            }
        }


        if (mesh)
        {
            printf("import Mesh\n");

            // to raw
            osg::ref_ptr<osg::Vec3Array> raw_vertex = new osg::Vec3Array();
            osg::ref_ptr<osg::Vec3Array> raw_normal = new osg::Vec3Array();
            osg::ref_ptr<osg::Vec4Array> raw_color = new osg::Vec4Array();
            osg::ref_ptr<osg::Vec2Array> raw_uv0 = new osg::Vec2Array();

            //get face index
            for (auto i = 0; i < mesh->num_faces(); i++)
            {
                draco::Mesh::Face f = mesh->face(draco::FaceIndex(i));

                int f0 = f[0].value();
                int f1 = f[1].value();
                int f2 = f[2].value();

                if (index_vertex->size() > 0)
                {
                    raw_vertex->push_back((*index_vertex)[f0]);
                    raw_vertex->push_back((*index_vertex)[f1]);
                    raw_vertex->push_back((*index_vertex)[f2]);
                }
                if (index_normal->size() > 0)
                {
                    raw_normal->push_back((*index_normal)[f0]);
                    raw_normal->push_back((*index_normal)[f1]);
                    raw_normal->push_back((*index_normal)[f2]);
                }
                if (index_color->size() > 0)
                {
                    raw_color->push_back((*index_color)[f0]);
                    raw_color->push_back((*index_color)[f1]);
                    raw_color->push_back((*index_color)[f2]);
                }
                if (index_uv0->size() > 0)
                {
                    raw_uv0->push_back((*index_uv0)[f0]);
                    raw_uv0->push_back((*index_uv0)[f1]);
                    raw_uv0->push_back((*index_uv0)[f2]);
                }
            }

            //new triangles geometry
            osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();
            if (raw_vertex->size() > 0)
            {
                geometry->setVertexArray(raw_vertex);
            }
            if (raw_normal->size() > 0)
            {
                geometry->setNormalArray(raw_normal);
                geometry->setNormalBinding(osg::Geometry::AttributeBinding::BIND_PER_VERTEX);
            }
            if (raw_uv0->size() > 0)
            {
                geometry->setTexCoordArray(0, raw_uv0);
            }
            if (raw_color->size() > 0)
            {
                geometry->setColorArray(raw_color);
                geometry->setColorBinding(osg::Geometry::BIND_PER_VERTEX);
            }

            if (raw_vertex->size() > 0)
            {
                geometry->addPrimitiveSet(
                    new osg::DrawArrays(osg::PrimitiveSet::TRIANGLES, 0, raw_vertex->size()));

                //geode
                osg::Geode* geode = new osg::Geode();
                geode->addDrawable(geometry);
                ret->addChild(geode);
            }


        }
        else
        {
            printf("import PointCloud\n");

            //new points geometry
            osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();
            if (index_vertex->size() > 0)
            {
                geometry->setVertexArray(index_vertex);
            }
            if (index_normal->size() > 0)
            {
                geometry->setNormalArray(index_normal);
                geometry->setNormalBinding(osg::Geometry::AttributeBinding::BIND_PER_VERTEX);
            }
            if (index_uv0->size() > 0)
            {
                geometry->setTexCoordArray(0, index_uv0);
            }
            if (index_color->size() > 0)
            {
                geometry->setColorArray(index_color);
                geometry->setColorBinding(osg::Geometry::BIND_PER_VERTEX);
            }

            if (index_vertex->size() > 0)
            {
                geometry->addPrimitiveSet(
                    new osg::DrawArrays(osg::PrimitiveSet::POINTS, 0, index_vertex->size()));

                //geode
                osg::Geode* geode = new osg::Geode();
                geode->addDrawable(geometry);
                ret->addChild(geode);
            }
        }

        return ret;
    }

    virtual WriteResult writeNode(const osg::Node& node, const std::string& fileName
        , const Options* options = NULL) const
    {
        std::string ext = osgDB::getLowerCaseFileExtension(fileName);
        if (!acceptsExtension(ext)) return WriteResult::FILE_NOT_HANDLED;

        OSG_INFO << "Writing file " << fileName << std::endl;


        DracoOptions draco_options;
        //draco_options.is_point_cloud = true; //default is false

        //detect node is mesh or point cloud
        DarocOptionsStruct dos = parseOptions(options);
        draco_options.is_point_cloud = dos.isPointCloud;


        //
        osg::ref_ptr<GeometryFlat> gf = new GeometryFlat();
        (const_cast<osg::Node*>(&node))->accept(*gf);


        //pointCloud and mesh
        std::unique_ptr<draco::PointCloud> pc;
        draco::Mesh *mesh = nullptr;
        if (!draco_options.is_point_cloud)
        {
            std::unique_ptr<draco::Mesh> out_mesh(new draco::Mesh());

            //osg geomtry to draco

            int num_positions_ = gf->m_geomtry_data.raw_vertex->size();
            int num_obj_faces_ = num_positions_ / 3;

            // Initialize point cloud and mesh properties.
            out_mesh->SetNumFaces(num_obj_faces_);
            out_mesh->set_num_points(num_positions_);

            osgNodeToDarocAttribute(gf, out_mesh.get());

            // Add faces with identity mapping between vertex and corner indices.
            // Duplicate vertices will get removed later.
            draco::Mesh::Face face;
            for (draco::FaceIndex i(0); i < num_obj_faces_; ++i)
            {
                for (int c = 0; c < 3; ++c)
                {
                    face[c] = 3 * i.value() + c;
                }
                out_mesh->SetFace(i, face);
            }

            //if (true/*deduplicate_input_values_*/) 
            //{
            out_mesh->DeduplicateAttributeValues();
            //}
            out_mesh->DeduplicatePointIds();

            mesh = out_mesh.get();
            pc = std::move(out_mesh);
        }
        else
        {
            //pc = draco::ReadPointCloudFromFile(draco_options.input);

            //osg points to draco
            pc = std::unique_ptr<draco::PointCloud>(new draco::PointCloud());

            int num_positions_ = gf->m_geomtry_data.raw_vertex->size();
            pc->set_num_points(num_positions_);

            osgNodeToDarocAttribute(gf, pc.get());

            pc->DeduplicateAttributeValues();
            pc->DeduplicatePointIds();

            if (!pc)
            {
                printf("Failed loading the input point cloud.\n");
                return WriteResult::ERROR_IN_WRITING_FILE;
            }
        }

        // Setup encoder options.
        draco::EncoderOptions encoder_options = draco::CreateDefaultEncoderOptions();
        if (draco_options.pos_quantization_bits > 0)
        {
            draco::SetNamedAttributeQuantization(&encoder_options, *pc.get(),
                draco::GeometryAttribute::POSITION,
                draco_options.pos_quantization_bits);
        }
        if (draco_options.tex_coords_quantization_bits > 0)
        {
            draco::SetNamedAttributeQuantization(&encoder_options, *pc.get(),
                draco::GeometryAttribute::TEX_COORD,
                draco_options.tex_coords_quantization_bits);
        }
        if (draco_options.normals_quantization_bits > 0)
        {
            draco::SetNamedAttributeQuantization(&encoder_options, *pc.get(),
                draco::GeometryAttribute::NORMAL,
                draco_options.normals_quantization_bits);
        }

        // Convert compression level to speed (that 0 = slowest, 10 = fastest).
        const int speed = 10 - draco_options.compression_level;
        draco::SetSpeedOptions(&encoder_options, speed, speed);

        if (draco_options.output.empty())
        {
            // Create a default output file by attaching .drc to the input file name.
            //draco_options.output = draco_options.input + ".drc";
            draco_options.output = fileName;
        }

        //is mesh
        bool is_mesh = false;
        is_mesh = (mesh && mesh->num_faces() > 0);

        PrintOptions(*pc.get(), draco_options);

        if (is_mesh)
        {
            EncodeMeshToFile(*mesh, encoder_options, draco_options.output);
        }
        else
        {
            EncodePointCloudToFile(*pc.get(), encoder_options, draco_options.output);
        }

        return WriteResult::FILE_SAVED;
    }
};

REGISTER_OSGPLUGIN(drc, ReaderWriterDRC)
