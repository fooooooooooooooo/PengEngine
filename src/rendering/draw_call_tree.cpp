#include "draw_call_tree.h"

#include <profiling/scoped_event.h>
#include <profiling/scoped_gpu_event.h>
#include <utils/strtools.h>
#include <utils/vectools.h>
#include <vector>

#include "draw_call.h"
#include "mesh.h"
#include "shader.h"
#include "material.h"
#include "render_queue_stats.h"
#include "utils.h"

using namespace rendering;

DrawCallTree::DrawCallTree(std::vector<DrawCall>&& draw_calls)
{
    SCOPED_EVENT("Building DrawCallTree", strtools::catf_temp("%d draw calls", draw_calls.size()));

    std::vector<DrawCall> draw_calls_ordered(std::move(draw_calls));
    std::ranges::sort(draw_calls_ordered,
        [](const DrawCall& x, const DrawCall& y)
        {
            return x.order < y.order;
        });

    for (DrawCall& draw_call : draw_calls_ordered)
    {
        check(draw_call.material);
        check(draw_call.mesh);

        if (draw_call.material->shader()->requires_blending())
        {
            add_blended_draw(std::move(draw_call));
        }
        else
        {
            add_opaque_draw(std::move(draw_call));
        }
    }

    std::ranges::sort(
        _shader_draws,
        [](const ShaderDrawTree& x, const ShaderDrawTree& y)
        {
            return x.shader->draw_order() < y.shader->draw_order();
        });

    merge_tree();
}

void DrawCallTree::execute(RenderQueueStats& stats) const
{
    SCOPED_EVENT("DrawCallTree - execute");
    SCOPED_GPU_EVENT("Draw Scene");

    for (const ShaderDrawTree& shader_draw : _shader_draws)
    {
        const peng::shared_ref<const Shader> shader = shader_draw.shader;

        SCOPED_GPU_EVENT(strtools::catf_temp("Shader - %s", shader->name().c_str()));
        shader_draw.shader->use();
        stats.shader_switches++;

        for (const MeshDrawTree& mesh_draw : shader_draw.mesh_draws)
        {
            const peng::shared_ref<const Mesh> mesh = mesh_draw.mesh;

            // TODO: we can skip a mesh switch if the mesh already happens to be bound
            //       from the previous shader draw
            SCOPED_GPU_EVENT(strtools::catf_temp("Mesh - %s", mesh->name().c_str()));
            mesh->bind();
            stats.mesh_switches++;

            for (const DrawCall& draw_call : mesh_draw.draw_calls)
            {
                check(draw_call.material);
                check(draw_call.material->shader() == shader_draw.shader);

                draw_call.material->apply_uniforms();
                draw_call.material->bind_buffers();

                if (draw_call.instance_count == 1)
                {
                    mesh->draw();
                }
                else
                {
                    mesh->draw_instanced(draw_call.instance_count);
                }

                stats.draw_calls++;
                stats.triangles += draw_call.instance_count * mesh->num_triangles();
            }
        }
    }
}

void DrawCallTree::add_opaque_draw(DrawCall&& draw_call)
{
    const peng::shared_ref<const Shader> shader = draw_call.material->shader();
    MeshDrawTree& mesh_draw = find_add_mesh_draw(shader, draw_call.mesh.to_shared_ref());
    mesh_draw.draw_calls.push_back(std::move(draw_call));
}

void DrawCallTree::add_blended_draw(DrawCall&& draw_call)
{
    // Only try merging with the last of each tree stage to preserve order
    ShaderDrawTree* shader_draw = vectools::try_back(_shader_draws);
    if (!shader_draw || shader_draw->shader != draw_call.material->shader())
    {
        shader_draw = &_shader_draws.emplace_back(ShaderDrawTree{
            .shader = draw_call.material->shader()
        });
    }

    MeshDrawTree* mesh_draw = vectools::try_back(shader_draw->mesh_draws);
    if (!mesh_draw || mesh_draw->mesh != draw_call.mesh)
    {
        mesh_draw = &shader_draw->mesh_draws.emplace_back(MeshDrawTree{
            .mesh = draw_call.mesh.to_shared_ref()
        });
    }

    mesh_draw->draw_calls.push_back(std::move(draw_call));
}

void DrawCallTree::merge_tree()
{
    SCOPED_EVENT("DrawCallTree - merge shader draws");

    _shader_draws = merge_shader_draws(std::move(_shader_draws));
    for (ShaderDrawTree& shader_draw : _shader_draws)
    {
        shader_draw.mesh_draws = merge_mesh_draws(std::move(shader_draw.mesh_draws));
    }
}

std::vector<ShaderDrawTree> DrawCallTree::merge_shader_draws(std::vector<ShaderDrawTree>&& shader_draws) const
{
    std::vector<ShaderDrawTree> merged_draws;

    for (ShaderDrawTree& shader_draw : shader_draws)
    {
        ShaderDrawTree* current = vectools::try_back(merged_draws);
        if (current && current->shader == shader_draw.shader)
        {
#ifdef WIN32
            current->mesh_draws.append_range(std::move(shader_draw.mesh_draws));
#else
            append_range(current->mesh_draws, std::move(shader_draw.mesh_draws));
#endif
        }
        else
        {
            merged_draws.push_back(std::move(shader_draw));
        }
    }

    return merged_draws;
}

std::vector<MeshDrawTree> DrawCallTree::merge_mesh_draws(std::vector<MeshDrawTree>&& mesh_draws) const
{
    std::vector<MeshDrawTree> merged_draws;

    for (MeshDrawTree& mesh_draw : mesh_draws)
    {
        MeshDrawTree* current = vectools::try_back(merged_draws);
        if (current && current->mesh == mesh_draw.mesh)
        {
#ifdef WIN32
            current->draw_calls.append_range(std::move(mesh_draw.draw_calls));
#else
            append_range(current->draw_calls, std::move(mesh_draw.draw_calls));
#endif
        }
        else
        {
            merged_draws.push_back(std::move(mesh_draw));
        }
    }

    return merged_draws;
}

ShaderDrawTree& DrawCallTree::find_add_shader_draw(const peng::shared_ref<const Shader>& shader)
{
    if (const auto it = _shader_draw_indices.find(shader); it != _shader_draw_indices.end())
    {
        return _shader_draws[it->second];
    }

    const ShaderDrawTree shader_draw = {
        .index = _shader_draws.size(),
        .shader = shader
    };

    _shader_draw_indices[shader] = shader_draw.index;
    _shader_draws.push_back(shader_draw);

    return _shader_draws[shader_draw.index];
}

MeshDrawTree& DrawCallTree::find_add_mesh_draw(const peng::shared_ref<const Shader>& shader,
    const peng::shared_ref<const Mesh>& mesh)
{
    const auto key = std::make_tuple(shader, mesh);
    if (const auto it = _mesh_draw_indices.find(key); it != _mesh_draw_indices.end())
    {
        const std::tuple<size_t, size_t> index_pair = it->second;
        return _shader_draws[std::get<0>(index_pair)].mesh_draws[std::get<1>(index_pair)];
    }

    ShaderDrawTree& shader_draw = find_add_shader_draw(shader);
    std::vector<MeshDrawTree>& mesh_draws = shader_draw.mesh_draws;

    const MeshDrawTree mesh_draw = {
        .index = mesh_draws.size(),
        .mesh = mesh
    };

    _mesh_draw_indices[key] = std::make_tuple(shader_draw.index, mesh_draw.index);
    mesh_draws.push_back(mesh_draw);

    return mesh_draws[mesh_draw.index];
}
