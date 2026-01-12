export function slugFromPathname(pathname: string): string | null {
  if (!pathname.startsWith("/effects/")) return null;
  const slug = pathname.substring("/effects/".length);
  return slug.length ? slug : null;
}

