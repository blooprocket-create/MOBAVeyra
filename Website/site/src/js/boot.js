// Runs before the page paints, so content that motion.js reveals is hidden from the first frame instead of
// flashing in and out. A file of its own because the site's content security policy allows no inline script.
document.documentElement.classList.add("js");
