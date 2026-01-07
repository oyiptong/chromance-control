import unittest


class TestGenerateLedmapTopology(unittest.TestCase):
    def test_vertex_ids_are_lexicographic_and_stable(self):
        # Import from the actual generator source-of-truth.
        from scripts.generate_ledmap import SEGMENTS

        vertices = sorted({v for seg in SEGMENTS for v in seg})
        self.assertGreaterEqual(len(vertices), 1)

        # Ensure lexicographic ordering by (vx, vy).
        self.assertEqual(vertices, sorted(vertices, key=lambda v: (v[0], v[1])))

        # Spot-check a couple IDs that are relied on in docs/debugging.
        # (These are stable for the canonical Chromance topology.)
        self.assertEqual(vertices[9], (2, 7))
        self.assertEqual(vertices[14], (3, 8))

    def test_seg_vertex_endpoints_are_in_range(self):
        from scripts.generate_ledmap import SEGMENTS

        vertices = sorted({v for seg in SEGMENTS for v in seg})
        vid = {v: i for i, v in enumerate(vertices)}

        for seg_id, (va, vb) in enumerate(SEGMENTS, start=1):
            self.assertIn(va, vid, f"seg {seg_id} endpoint A missing from vertex table")
            self.assertIn(vb, vid, f"seg {seg_id} endpoint B missing from vertex table")
            self.assertTrue(0 <= vid[va] < len(vertices))
            self.assertTrue(0 <= vid[vb] < len(vertices))

        # Spot-check seg 19 mapping: A=(2,7)->V9, B=(3,8)->V14.
        self.assertEqual(SEGMENTS[18], ((2, 7), (3, 8)))
        self.assertEqual(vid[(2, 7)], 9)
        self.assertEqual(vid[(3, 8)], 14)


if __name__ == "__main__":
    unittest.main()

