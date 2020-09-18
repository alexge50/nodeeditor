#include <Events.h>
#include <NodeEditor.h>
#include <detail/Compute.h>

#include <algorithm>
#include <math.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

static std::optional<NodeId> get_containing_node(const Graph& graph, glm::vec2 point)
{
    auto node = std::find_if(
            graph.nodes.begin(),
            graph.nodes.end(),
            [&graph, &point](const auto& node) {
                const NodeCompute& node_compute = graph.node_computed.at(node.first);
                return contains_point(node_compute.bounding_box, point);
            });

    if(node == graph.nodes.end())
        return std::nullopt;
    else return node->first;
}

void process(NodeEditor& node_editor, const std::vector<InputEvent>& input, std::vector<EditorEvent>&)
{
    for(const auto& event: input)
    {
        std::visit(
                overloaded {
                    [&](const InputEvents::Click& click) {
                        auto canvas_position = to_world_space(node_editor.camera, glm::vec2{click.x, click.y});

                        if(auto clicked_node = get_containing_node(node_editor.graph, canvas_position.get()); clicked_node)
                        {
                            if(!click.special_key)
                                node_editor.selected_nodes.clear();

                            node_editor.selected_nodes.insert(*clicked_node);
                        }
                        else node_editor.selected_nodes.clear();
                    },
                    [](InputEvents::Delete) {

                    },
                    [&](const InputEvents::DragBegin& drag_begin) {
                        auto canvas_position = to_world_space(node_editor.camera, glm::vec2{drag_begin.x, drag_begin.y});

                        if(auto dragged_node = get_containing_node(node_editor.graph, canvas_position.get()); dragged_node)
                        {
                            if(!drag_begin.special_key)
                            {
                                node_editor.selected_nodes.clear();
                                node_editor.selected_nodes.insert(*dragged_node);
                            }

                            node_editor.drag_state = NodeDrag {
                                    canvas_position.get()
                            };
                        }
                        else
                        {
                            if(drag_begin.special_key)
                                node_editor.drag_state = SelectDrag {
                                    {drag_begin.x, drag_begin.y},
                                    {drag_begin.x, drag_begin.y}
                                };
                            else
                                node_editor.drag_state = ViewDrag {
                                    {drag_begin.x, drag_begin.y}
                                };
                        }
                    },
                    [&](InputEvents::DragEnd) {
                        if(std::holds_alternative<SelectDrag>(node_editor.drag_state))
                        {
                            auto& select_drag = std::get<SelectDrag>(node_editor.drag_state);

                            glm::vec2 p[2] = {
                                to_world_space(node_editor.camera, select_drag.current_corner).get(),
                                to_world_space(node_editor.camera, select_drag.begin_corner).get(),
                            };

                            glm::vec2 box_size = {
                                    fabsf(p[0].x - p[1].x),
                                    fabsf(p[0].y - p[1].y)
                            };

                            glm::vec2 box_position = (p[0] + p[1]) / 2.f;


                            for(const auto&[id, node]: node_editor.graph.nodes)
                            {
                                if(contains_point(compute_bounding_box(box_position, box_size), node.position))
                                    node_editor.selected_nodes.insert(id);
                            }
                        }

                        node_editor.drag_state = NoDrag{};
                    },
                    [&](const InputEvents::DragSustain& drag_sustain) {
                        if(std::holds_alternative<NodeDrag>(node_editor.drag_state))
                        {
                            auto& node_drag = std::get<NodeDrag>(node_editor.drag_state);

                            auto canvas_position = to_world_space(node_editor.camera, glm::vec2{drag_sustain.x, drag_sustain.y});

                            glm::vec2 delta = canvas_position - node_drag.begin_position;
                            node_drag.begin_position = canvas_position.get();

                            for(const auto& node: node_editor.selected_nodes)
                            {
                                node_editor.graph.nodes[node].position += delta;
                                compute_node(node_editor, node);
                            }
                        }

                        if(std::holds_alternative<SelectDrag>(node_editor.drag_state))
                        {
                            auto& select_drag = std::get<SelectDrag>(node_editor.drag_state);

                            select_drag.current_corner = {
                                drag_sustain.x,
                                drag_sustain.y
                            };
                        }

                        if(std::holds_alternative<ViewDrag>(node_editor.drag_state))
                        {
                            auto& node_drag = std::get<ViewDrag>(node_editor.drag_state);

                            glm::vec2 delta = glm::vec2{drag_sustain.x, drag_sustain.y} - node_drag.begin_position;
                            node_drag.begin_position = glm::vec2{drag_sustain.x, drag_sustain.y};

                            node_editor.camera.position -= delta;
                        }
                    },
                    [&](const InputEvents::Scroll& scroll) {
                        node_editor.camera.zoom += scroll.value;
                        node_editor.camera.zoom = glm::clamp(node_editor.camera.zoom, 0.1f, 3.f);
                    }
                },
                event
        );
    }
}