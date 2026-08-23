# ADR-0001: Preserve the Product Baseline and Migrate Incrementally

- Status: accepted
- Date: 2026-08-22

## Context

A previously validated Windows candidate is referenced by the planning material, while this workspace initially contains planning documents only. The first deliverable has a two-day deadline and no physical RC003 is currently available.

## Decision

Use working Windows behavior as the product baseline when it becomes available. Build C++20 seams and offline tests without replacing working production modules during the two-day sprint.

## Consequences

- Delivery work remains focused on a runnable product.
- C++ migration occurs module by module after behavior is captured.
- Hardware claims remain deferred until a physical RC003 is available.

## Rejected alternative

A greenfield rewrite would discard working evidence and cannot be credibly validated within the current deadline.

