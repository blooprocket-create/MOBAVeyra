"""Spring chains for a body's loose parts (ADR-069 §7), shared by the archetypes that hang them. An archetype names its
loose parts (parts: {part: [(chain, its bones root to tip, the bone it hangs from)]}) and lays their bones out; a kit
entry's springs choose which it wears and how each moves."""


def bones_with(base, parts, spec):
    """base (an archetype's bones, (name, parent)) and the chains of the loose parts spec's springs name, each chain's
    bones after the bone it hangs from."""
    bones, known = list(base), {name for name, _ in base}
    for part in spec.get("springs", {}):
        for _chain, names, parent in parts[part]:
            for name in names:
                if name not in known:
                    bones.append((name, parent))
                    known.add(name)
                parent = name
    return bones


def records(parts, spec, L, colliders):
    """The body art's record of its loose parts: each chain with its part's spring, and the capsules the chains hang
    outside (colliders: from, to, radius); None when it wears none. No chain's joint may rest inside a capsule, or the
    engine would push its cloth off where the model hangs it."""
    springs = spec.get("springs", {})
    chains = [{"bones": names, "stiffness": springs[part]["stiffness"], "drag": springs[part]["drag"], "damping": springs[part]["damping"],
               "maxAngle": springs[part]["maxAngle"]}
              for part in springs for _chain, names, _parent in parts[part]]
    for collider in colliders:
        a, b = L[collider["from"]][0], L[collider["to"]][0]
        ab = b - a
        for chain in chains:
            for bone in chain["bones"][1:]:
                p = L[bone][0]
                nearest = a + ab * max(0.0, min(1.0, (p - a).dot(ab) / max(ab.dot(ab), 1e-9)))
                assert (p - nearest).length >= collider["radius"], (spec["id"], bone, "rests inside its body", collider)
    return {"chains": chains, "colliders": colliders} if chains else None
