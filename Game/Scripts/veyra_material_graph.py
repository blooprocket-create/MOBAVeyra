"""Shared helpers for the Crucible's generated materials (ADR-040): a node layout over MaterialEditingLibrary, shared-
sampler texture samples and pin linking. Imported by the editor-only art importers in this folder."""
import unreal

EDIT = unreal.MaterialEditingLibrary

class Graph:
    """A small helper over MaterialEditingLibrary that lays nodes out in columns."""

    def __init__(self, material):
        self.material = material
        self.rows = {}

    def node(self, cls, column, **props):
        row = self.rows.get(column, 0)
        self.rows[column] = row + 1
        expression = EDIT.create_material_expression(self.material, cls, -2200 + column * 320, -1200 + row * 140)
        for key, value in props.items():
            expression.set_editor_property(key, value)
        return expression

    def scalar(self, column, name, value, group):
        return self.node(unreal.MaterialExpressionScalarParameter, column, parameter_name=name, default_value=value, group=group)

    def link(self, source, output, target, input_name):
        assert EDIT.connect_material_expressions(source, output, target, input_name), f"{output} -> {input_name}"

    def op(self, cls, column, a, b, a_out="", b_out=""):
        expression = self.node(cls, column)
        self.link(a, a_out, expression, "A")
        self.link(b, b_out, expression, "B")
        return expression


def sample(graph, column, texture, uv, sampler_type):
    expression = graph.node(unreal.MaterialExpressionTextureSample, column, texture=texture, sampler_type=sampler_type,
                            sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
    graph.link(uv, "", expression, "UVs")
    return expression


def link_named(g, source, output, target, names):
    """Links to the first of `names` the target answers to: custom outputs name pins by property or display name."""
    for name in names:
        if EDIT.connect_material_expressions(source, output, target, name):
            return
    raise AssertionError(f"none of {names}")
