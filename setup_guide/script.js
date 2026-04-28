(function () {
  'use strict';

  const topBtn = document.getElementById('topBtn');
  if (topBtn) {
    const toggle = () => {
      topBtn.hidden = window.scrollY < 600;
    };
    window.addEventListener('scroll', toggle, { passive: true });
    topBtn.addEventListener('click', () => {
      window.scrollTo({ top: 0, behavior: 'smooth' });
    });
    toggle();
  }

  const steps = document.querySelectorAll('.step');
  if ('IntersectionObserver' in window && steps.length) {
    const tocLinks = document.querySelectorAll('.toc a');
    const linkById = new Map();
    tocLinks.forEach(a => {
      const id = a.getAttribute('href').slice(1);
      linkById.set(id, a);
    });

    const observer = new IntersectionObserver((entries) => {
      entries.forEach(entry => {
        const link = linkById.get(entry.target.id);
        if (!link) return;
        if (entry.isIntersecting) {
          tocLinks.forEach(a => a.removeAttribute('aria-current'));
          link.setAttribute('aria-current', 'true');
        }
      });
    }, { rootMargin: '-30% 0px -60% 0px' });

    steps.forEach(s => observer.observe(s));
  }
})();
