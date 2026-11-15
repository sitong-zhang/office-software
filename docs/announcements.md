# In-app announcements

Announcements are messages delivered inside the application without shipping a release. They are read from this repository at startup, shown once, and dismissed with **继续** (Continue) before the application opens.

## Source of truth

```text
announcements.json          (repository root, branch main)
```

The client reads the raw file:

```text
https://raw.githubusercontent.com/sitong-zhang/office-software/main/announcements.json?t=<unix-ms>
```

The `t` parameter is a cache-buster; without it, the raw CDN may serve a stale copy for several minutes.

## Schema

```json
{
  "schema_version": 1,
  "updated_at": "2026-11-15T12:00:00+08:00",
  "announcements": [
    {
      "id": "2026-11-15-project-start",
      "title": "Chenyi Office（胡辰逸定制版）项目启动",
      "body": "First paragraph.\n\nSecond paragraph.",
      "severity": "info",
      "published_at": "2026-11-15T12:00:00+08:00",
      "expires_at": null,
      "min_version": null,
      "max_version": null,
      "action_label": "查看路线图",
      "action_url": "https://github.com/sitong-zhang/office-software/blob/main/docs/roadmap.md"
    }
  ]
}
```

| Field | Required | Meaning |
| --- | --- | --- |
| `schema_version` | yes | Currently `1` |
| `updated_at` | yes | Last edit time, ISO 8601 with offset |
| `announcements[].id` | yes | Stable unique id; never reuse or change it, since clients remember what they have seen |
| `title` | yes | Short headline, plain text |
| `body` | yes | Plain text; `\n` for line breaks; keep under ~2000 characters |
| `severity` | yes | `info` or `important`; controls emphasis only |
| `published_at` | yes | Not shown before this time |
| `expires_at` | no | Hidden after this time; `null` means no expiry |
| `min_version` / `max_version` | no | Inclusive client version range; `null` means no bound |
| `action_label` / `action_url` | no | Optional link opened in the browser; it never replaces **继续** |

## Client behavior

1. Startup begins with the update check (see [updater.md](updater.md)).
2. The announcement feed is fetched even when an update is available, so deferring an update does not hide release notices.
3. Otherwise the client fetches the announcement feed with a 10 s timeout and 2 attempts.
4. Entries are filtered by publication time, expiry, client version range, and the local "seen" set.
5. If at least one entry remains, a full-window announcement screen lists them in publication order. The only action is **继续**; there is no skip link.
6. **继续** records the shown ids locally and opens the application. The screen is shown once per id per installation.
7. If the fetch fails, the application opens normally and retries on the next launch. Announcements never block startup on network failure.
8. The seen set is stored locally per user profile and survives updates.

## Publishing an announcement

1. Edit `announcements.json` directly on `main` or through a pull request.
2. Give the entry a new, unique `id` (date plus short slug).
3. Run the validation workflow or locally:

   ```bash
   python -m json.tool announcements.json > /dev/null
   ```

4. Commit. No release is required; the next client launch picks it up, allowing a few minutes for CDN propagation.
5. Verify with:

   ```bash
   curl -s 'https://raw.githubusercontent.com/sitong-zhang/office-software/main/announcements.json?t=1' | head -40
   ```

## Content rules

- No credentials, personal data, or links outside this repository's releases and documentation.
- `important` is reserved for release notices, security notices, and service-impacting changes.
- Expired entries may be removed after 90 days; keep at least the most recent entry.
- Announcements are not a substitute for release notes: version-specific details belong in the release.

## Acceptance criteria

- [ ] A new entry appears once on the next launch and not again.
- [ ] **继续** opens the application.
- [ ] Multiple new entries appear on a single screen in publication order.
- [ ] Announcements remain visible when an update is deferred.
- [ ] Malformed or unreachable feed never blocks startup.
