#include "ObjLoader.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

ObjLoader::ObjLoader(VkPhysicalDevice physical_device, VkDevice logical_device, VkQueue transfer_queue, VkCommandPool command_pool)
{
	this->physical_device = physical_device;
	this->logical_device = logical_device;
	this->transfer_queue = transfer_queue;
	this->command_pool = command_pool;
}

std::shared_ptr<Model> ObjLoader::load_model(std::string modelFile, std::vector<int> matToTex)
{
    std::shared_ptr<Model> new_model = std::make_shared<Model>();

    tinyobj::ObjReaderConfig reader_config;
    //reader_config.mtl_search_path = ""; // Path to material files

    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(modelFile, reader_config)) {
        if (!reader.Error().empty()) {
            std::cerr << "TinyObjReader: " << reader.Error();
        }
        exit(EXIT_FAILURE);
    }

    if (!reader.Warning().empty()) {
        std::cout << "TinyObjReader: " << reader.Warning();
    }

    auto& attrib = reader.GetAttrib();
    auto& shapes = reader.GetShapes();
    auto& materials = reader.GetMaterials();

    std::unordered_map<Vertex, uint32_t> vertices_map{};
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    // Loop over shapes
    for (size_t s = 0; s < shapes.size(); s++) {

        // Loop over faces(polygon)
        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
            size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);

            // Loop over vertices in the face.
            for (size_t v = 0; v < fv; v++) {
                // access to vertex
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];
                glm::vec3 pos = { vx,vy,vz };

                glm::vec3 normals(0.0f);
                // Check if `normal_index` is zero or positive. negative = no normal data
                if (idx.normal_index >= 0) {
                    tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
                    tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
                    tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];
                    normals = glm::vec3(nx, ny, nz);
                }

                glm::vec2 tex_coords(0.0f);
                // Check if `texcoord_index` is zero or positive. negative = no texcoord data
                if (idx.texcoord_index >= 0) {
                    tinyobj::real_t tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
                    tinyobj::real_t ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
                    tex_coords = glm::vec2(tx, ty);
                }

                Vertex vert;// (pos, tex_coords, normals);
                vert.pos = pos;
                vert.texture_coords = tex_coords;
                vert.normal = normals;
                vert.mat_id.x = matToTex[shapes[s].mesh.material_ids[f]];
                //if(shapes[s].mesh.material_ids[f] == 24) std::cout << shapes[s].mesh.material_ids[f] << endl;
                /* vertices.push_back(vert);
                indices.push_back(indices.size());*/

                if (vertices_map.count(vert) == 0) {

                    vertices_map[vert] = vertices.size();
                    vertices.push_back(vert);

                }

                indices.push_back(vertices_map[vert]);

            }

            index_offset += fv;

            // per-face material
            shapes[s].mesh.material_ids[f];
        }
    }

    new_model->add_new_mesh(physical_device, logical_device, transfer_queue,
                            command_pool, vertices, indices);

	return new_model;
}

std::vector<std::string> ObjLoader::load_textures(std::string modelFile)
{
    tinyobj::ObjReaderConfig reader_config;
    //reader_config.mtl_search_path = ""; // Path to material files

    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(modelFile, reader_config)) {
        if (!reader.Error().empty()) {
            std::cerr << "TinyObjReader: " << reader.Error();
        }
        exit(EXIT_FAILURE);
    }

    if (!reader.Warning().empty()) {
        std::cout << "TinyObjReader: " << reader.Warning();
    }

    auto& attrib = reader.GetAttrib();
    auto& shapes = reader.GetShapes();
    auto& materials = reader.GetMaterials();

    std::vector<std::string> texture_list;
    texture_list.reserve(materials.size());

    // we now iterate over all materials to get diffuse textures

    for (size_t i = 0; i < materials.size(); i++) {
        const tinyobj::material_t* mp = &materials[i];

        if (mp->diffuse_texname.length() > 0) {

            std::string relative_texture_filename = mp->diffuse_texname;
            std::string texture_filename = get_base_dir(modelFile) + "/textures/" + relative_texture_filename;

            texture_list.push_back(texture_filename);

        }
        else {

            texture_list.push_back("");

        }
    }

    return texture_list;
}