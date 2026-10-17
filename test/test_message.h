#ifndef PCO_TEST_MESSAGE_H
#define PCO_TEST_MESSAGE_H
#include <greatest/greatest.h>
#include <connection.h>

TEST serde_package_range_t(void) {
    range_t range = {2, 0, 4, 3};
    range_t *output = (range_t *)malloc(sizeof(range_t));
    
    char *buf = (char *)malloc(4 * sizeof(int32_t));
    package_range_t(&range, buf);
    unpackage_range_t(output, buf);

    ASSERT_EQ(2, output->start_pos);
    ASSERT_EQ(0, output->start_char);
    ASSERT_EQ(4, output->end_pos);
    ASSERT_EQ(3, output->end_char);
    
    free(buf);
    free(output);

    PASS();
}

TEST serde_package_fline_t(void) {
    range_t range = {2, 0, 4, 3};
    fline_t line = {.kind = DIFF_ADDED, .range = range, .len = 6, .body = "hello"};
    fline_t *output = (fline_t *)malloc(sizeof(fline_t));

    char *buf = (char *)malloc(4 + sizeof(range_t) + 4 + 6);
    package_fline_t(&line, buf);
    unpackage_fline_t(output, buf);


    ASSERT_EQ("hello", output->body);
    ASSERT_EQ(DIFF_ADDED, output->kind);
    ASSERT_EQ(6, output->len);
    ASSERT_EQ(2, output->range.start_pos);
    ASSERT_EQ(0, output->range.start_char);
    ASSERT_EQ(4, output->range.end_pos);
    ASSERT_EQ(3, output->range.end_char);

    free(buf);
    free(output);

    PASS();
}

TEST serde_package_fdiff_t(void) {
    range_t range1 = {0, 0, 0, 5};
    range_t range2 = {1, 0, 1, 5};

    fline_t lines[2];
    lines[0].body = "this is a long line of text for testing";
    lines[0].len = 40;
    lines[0].kind = DIFF_ADDED;
    lines[0].range = range1;

    lines[1].body = "this is yet another a longer line of text";
    lines[1].len = 42;
    lines[1].kind = DIFF_MODIFIED;
    lines[1].range = range2;

    fdiff_t diff = {.path = "test.txt", .plen = 9, .cursor_r = 10, .llen = 2, .lines = lines};
    fdiff_t *output = (fdiff_t *)malloc(sizeof(fdiff_t));
    char *buf = (char *)malloc(9 + 4 + 4 + 4 + (40 + 4 + 4 + sizeof(range_t)) + (42 + 4 + 4 + sizeof(range_t)));

    package_fdiff_t(&diff, buf);
    unpackage_fdiff_t(output, buf);

    ASSERT_EQ("test.txt", output->path);
    ASSERT_EQ(9, output->plen);
    ASSERT_EQ(10, output->cursor_r);
    ASSERT_EQ(2, output->llen);

    ASSERT_EQ("this is a long line of text for testing", output->lines->body);
    ASSERT_EQ(40, output->lines->len);
    ASSERT_EQ(DIFF_ADDED, output->lines->kind);

    ASSERT_EQ(0, output->lines->range.start_pos);
    ASSERT_EQ(0, output->lines->range.start_char);
    ASSERT_EQ(0, output->lines->range.end_pos);
    ASSERT_EQ(5, output->lines->range.end_char);

    ASSERT_EQ("this is yet another a longer line of text", (output->lines + 1)->body);
    ASSERT_EQ(42, (output->lines + 1)->len);
    ASSERT_EQ(DIFF_MODIFIED, (output->lines + 1)->kind);
    
    ASSERT_EQ(1, (output->lines + 1)->range.start_pos);
    ASSERT_EQ(0, (output->lines + 1)->range.start_char);
    ASSERT_EQ(1, (output->lines + 1)->range.end_pos);
    ASSERT_EQ(5, (output->lines + 1)->range.end_char);

    free(buf);
    free(output);

    PASS();
}

TEST serde_package_message_t(void) {
    range_t range1 = {4, 5, 6, 5};
    range_t range2 = {11, 4, 13, 5};

    fline_t lines[2];
    lines[0].body = "this is a long line of text for testing";
    lines[0].len = 40;
    lines[0].kind = DIFF_ADDED;
    lines[0].range = range1;

    lines[1].body = "this is yet another a longer line of text";
    lines[1].len = 42;
    lines[1].kind = DIFF_MODIFIED;
    lines[1].range = range2;

    fdiff_t diff = {.path = "test.txt", .plen = 9, .cursor_r = 10, .llen = 2, .lines = lines};
    message_t msg = {.kind = MSG_CONTENT, .dlen = 1, .diff = &diff};

    char *buf = package_message(&msg);
    message_t *output = unpackage_message(buf + 4); // skip header

    ASSERT_EQ(MSG_CONTENT, output->kind);
    ASSERT_EQ(1, output->dlen);
    ASSERT_EQ("test.txt", output->diff->path);
    ASSERT_EQ(10, output->diff->cursor_r);
    ASSERT_EQ(2, output->diff->llen);
    ASSERT_EQ(9, output->diff->plen);

    ASSERT_EQ("this is a long line of text for testing", output->diff->lines->body);
    ASSERT_EQ(40, output->diff->lines->len);
    ASSERT_EQ(DIFF_ADDED, output->diff->lines->kind);

    ASSERT_EQ(4, output->diff->lines->range.start_pos);
    ASSERT_EQ(5, output->diff->lines->range.start_char);
    ASSERT_EQ(6, output->diff->lines->range.end_pos);
    ASSERT_EQ(5, output->diff->lines->range.end_char);

    ASSERT_EQ("this is yet another a longer line of text", (output->diff->lines + 1)->body);
    ASSERT_EQ(42, (output->diff->lines + 1)->len);
    ASSERT_EQ(DIFF_MODIFIED, (output->diff->lines + 1)->kind);
    
    ASSERT_EQ(11, (output->diff->lines + 1)->range.start_pos);
    ASSERT_EQ(4, (output->diff->lines + 1)->range.start_char);
    ASSERT_EQ(13, (output->diff->lines + 1)->range.end_pos);
    ASSERT_EQ(5, (output->diff->lines + 1)->range.end_char);

    PASS();
}

SUITE(suite_message_serde) {
    RUN_TEST(serde_package_range_t);
    RUN_TEST(serde_package_fline_t);
    RUN_TEST(serde_package_fdiff_t);
    RUN_TEST(serde_package_message_t);
}

#endif