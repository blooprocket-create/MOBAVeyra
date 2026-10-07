// The account rules a form checks before it asks anyone: the display-name rules the backend keeps
// (ADR-038 §4, provisional), from site.json, so a bad name never creates a Firebase user.

/** Why a display name breaks the rules: "length", "characters", or nothing. */
export function displayNameProblems(name, rules) {
  const problems = [];
  const length = [...name].length;
  if (length < rules.minLength || length > rules.maxLength) {
    problems.push("length");
  }
  if (name !== "" && !new RegExp(rules.pattern).test(name)) {
    problems.push("characters");
  }
  return problems;
}

export function isValidDisplayName(name, rules) {
  return displayNameProblems(name, rules).length === 0;
}

/** The rule in words, for a form's hint. */
export function describeDisplayName(rules) {
  return `${rules.minLength}–${rules.maxLength} letters, digits or underscores`;
}

/** A light check that an email looks like one; Firebase has the last word. */
export function looksLikeEmail(email) {
  return /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email);
}
