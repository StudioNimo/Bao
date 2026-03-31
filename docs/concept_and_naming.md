# Bao: development concept and naming

## 1. Background: the “reproducibility” problem in LLM work

When building systems on LLMs—especially agent-style flows that go from voice input to OS actions—engineers face a recurring dilemma:

- **Non-deterministic behavior:** Tweaking a prompt may improve one test case while another degrades unexpectedly.
- **Broken evaluation context:** “Which prompt,” “which parameters,” “which dataset,” and “which human rating” are often tracked in separate places.
- **Git’s limits:** Git excels at text (code), but it was not designed to tie LLM outputs and human judgments to a single timeline.

Bao exists to **track the intent behind prompt changes and the resulting accuracy improvements** in a reproducible way.

---

## 2. Why “Bao” (the steamed-bun metaphor)

The name reflects a metaphor for how the system works—not just a cute sound.

### “Wrapping many ingredients in one”

A bao wraps varied fillings in one skin to make a single dish.

- **Ingredients:** Prompt text, model name, parameters like temperature, test datasets.
- **Wrapper (skin):** The commit hash that freezes those pieces as a single immutable snapshot.
- **Steamer:** The repository (`.bao/`) that holds history.

By “wrapping” scattered pieces into one snapshot, developers can replay and compare past “flavors” (accuracy).

---

## 3. Design: how the CLI should feel

As a daily-driver CLI, we care about ergonomics as well as features.

- **Typing flow:** `b` (left) → `a` (left) → `o` (right) keeps movement small from the home row.
- **Git-like feel:** Out of respect for Linus Torvalds’ ideas, we implemented a native C tool so startup feels instant—no script interpreter “warm-up” getting in the way of thinking.

> **“Bao turns prompts—uncertain by nature—into durable assets.”**
