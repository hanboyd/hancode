"""Phase 1 migration scaffold (ADR-0011): module-level implementation
choice for the C++ / CPython coexistence and fallback framework.

This file is the *framework* only. No real product module is migrated
here; per migration plan section 1 rule 4 every known module keeps the
``"python"`` default until phase 8. Phase 2 will be the first module
that opts into ``"shadow"`` (ATVV capability parsing / ADPCM decoding)
because it is the side-effect-free compute path the roadmap explicitly
calls out.

Public API:

* ``implementation_choice(module_name) -> "python" | "native" | "shadow"``
* ``choose_implementation(module_name, python_impl, native_impl, *,
  side_effect_free=False)`` returns a callable dispatching to the
  chosen implementation, with a strict ``shadow`` mode reserved for
  side-effect-free compute modules.

Override sources, in lookup order:

1. Environment variable ``REMOTEMIC_NATIVE_CHOICE_<MODULE_NAME>``
   (uppercased module name, value one of ``python`` / ``native`` /
   ``shadow``).
2. The internal default policy table (currently empty - everything is
   ``python`` until phase 2 explicitly opts in).
3. ``"python"`` - the authoritative fallback for the entire migration
   window per plan section 1 rule 4.
"""

from __future__ import annotations

import os
from typing import Callable, Literal, TypeVar

ImplementationChoice = Literal["python", "native", "shadow"]

T = TypeVar("T")

# Default policy. Empty by design: every module is "python" until an
# explicit opt-in is added here (or via env var) for that module.
#
# ``atvv_protocol`` is the first side-effect-free compute module to
# register (Phase 2 / Area 1, ADR-0012 §6). The default stays
# ``"python"`` here per the migration plan §1 rule 4; only the env
# override or a shadow parity test selects ``"shadow"``. The entry
# below exists so a typo in the env var name (``..._ATVV_PROTO`` vs
# ``..._ATVV_PROTOCOL``) fails loud at ``implementation_choice`` rather
# than silently falling back to python.
_DEFAULT_CHOICES: dict[str, ImplementationChoice] = {
    "atvv_protocol": "python",
    "atvv_control_parse": "python",
    "atvv_control_encode": "python",
    "adpcm_ima_decode": "python",
    "adpcm_dc_highpass": "python",
    "adpcm_postprocess": "python",
    "adpcm_frame_accumulator": "python",
    # Phase 3 / ADR-0013 §3.1-§3.3: voice controller, release-window
    # debouncer, and ATVV session state machines. All three keep the
    # default "python" until step 5 flips a single-session owner.
    "voice_controller": "python",
    "voice_edge_debouncer": "python",
    "atvv_session": "python",
}

_ENV_PREFIX = "REMOTEMIC_NATIVE_CHOICE_"
_VALID_CHOICES: frozenset[str] = frozenset(("python", "native", "shadow"))


def implementation_choice(module_name: str) -> ImplementationChoice:
    """Return the active implementation choice for ``module_name``.

    Lookup order is env override -> default policy -> ``"python"``.
    An invalid env value is rejected loudly rather than silently
    defaulting, so a typo in a CI variable never silently flips a
    side-effecting module to the wrong implementation.
    """
    if not module_name:
        raise ValueError("module_name must not be empty")

    env_key = _ENV_PREFIX + module_name.upper()
    env_value = os.environ.get(env_key)
    if env_value is not None:
        normalized = env_value.strip().lower()
        if normalized not in _VALID_CHOICES:
            raise ValueError(
                f"{env_key}={env_value!r}: must be one of "
                f"{sorted(_VALID_CHOICES)}"
            )
        return normalized  # type: ignore[return-value]

    return _DEFAULT_CHOICES.get(module_name, "python")


def choose_implementation(
    module_name: str,
    python_impl: Callable[..., T],
    native_impl: Callable[..., T],
    *,
    side_effect_free: bool = False,
) -> Callable[..., T]:
    """Return a callable that dispatches to the chosen implementation.

    ``"python"`` -> ``python_impl``.
    ``"native"`` -> ``native_impl`` (the caller is responsible for the
    native extension being importable; if it is not, the ``ImportError``
    raised inside ``native_impl`` propagates to the caller, who must
    treat that the same way as the dry-run fallback does).
    ``"shadow"`` -> requires ``side_effect_free=True``; returns a
    wrapper that runs both, compares results, and raises ``RuntimeError``
    on any mismatch or native-side exception. ``shadow`` is reserved for
    side-effect-free compute modules (ATVV / ADPCM in phase 2) so the
    comparison never silently hides a divergent side effect.
    """
    choice = implementation_choice(module_name)

    if choice == "python":
        return python_impl
    if choice == "native":
        return native_impl
    if choice == "shadow":
        if not side_effect_free:
            raise RuntimeError(
                f"shadow mode requested for {module_name!r} but "
                f"side_effect_free=False; shadow is reserved for "
                f"compute-only modules"
            )

        def _shadow(*args: object, **kwargs: object) -> T:
            py_result = python_impl(*args, **kwargs)
            try:
                native_result = native_impl(*args, **kwargs)
            except Exception as exc:  # noqa: BLE001 - intentionally broad
                raise RuntimeError(
                    f"shadow({module_name}): native impl raised "
                    f"{type(exc).__name__}"
                ) from exc
            if py_result != native_result:
                raise RuntimeError(
                    f"shadow({module_name}): python/native result mismatch"
                )
            return py_result

        return _shadow  # type: ignore[return-value]

    raise RuntimeError(f"unknown implementation choice: {choice!r}")


__all__ = [
    "ImplementationChoice",
    "implementation_choice",
    "choose_implementation",
]