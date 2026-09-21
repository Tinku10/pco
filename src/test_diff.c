#include <greatest/greatest.h>
#include "diff.h"


TEST first_time_diff_should_contain_all_lines_of_kind_added(void) {
    file_state* first = (file_state*) malloc(sizeof(file_state));
    first->len = 3;
    file_line* lines = (file_line*)malloc(sizeof(file_line) * first->len);
    (lines + 0)->content = "this is the first line";
    (lines + 0)->len = 22;

    (lines + 1)->content = "this is the second line";
    (lines + 1)->len = 23;

    (lines + 2)->content = "this is the third and the final line";
    (lines + 2)->len = 36;
    first->lines = lines;

    file_state* empty = (file_state*) malloc(sizeof(file_state));
    empty->len = 0;
    empty->lines = NULL;
    fdiff_t* diff = (fdiff_t*) malloc(sizeof(fdiff_t));

    diff_files(empty, first, diff);

    // 3 lines of diff
    ASSERT_EQ(3, diff->llen);
    ASSERT_STR_EQ("this is the first line", diff->lines->body);
    ASSERT_EQ(22, diff->lines->len);
    ASSERT_EQ(0, diff->lines->num);
    ASSERT_ENUM_EQ(DIFF_ADDED, diff->lines->kind, diff_kind_str);
    ASSERT_STR_EQ("this is the second line", (diff->lines + 1)->body);
    ASSERT_EQ(23, (diff->lines + 1)->len);
    ASSERT_EQ(1, (diff->lines + 1)->num);
    ASSERT_ENUM_EQ(DIFF_ADDED, (diff->lines + 1)->kind, diff_kind_str);
    ASSERT_STR_EQ("this is the third and the final line", (diff->lines + 2)->body);
    ASSERT_EQ(36, (diff->lines + 2)->len);
    ASSERT_EQ(2, (diff->lines + 2)->num);
    ASSERT_ENUM_EQ(DIFF_ADDED, (diff->lines + 2)->kind, diff_kind_str);
    PASS();
}

TEST diff_called_on_identical_files_should_not_produce_any_diff(void) {
    file_state* first = (file_state*) malloc(sizeof(file_state));
    first->len = 3;
    file_line* lines1 = (file_line*)malloc(sizeof(file_line) * first->len);
    (lines1 + 0)->content = "this is the first line";
    (lines1 + 0)->len = 22;

    (lines1 + 1)->content = "this is the second line";
    (lines1 + 1)->len = 23;

    (lines1 + 2)->content = "this is the third and the final line";
    (lines1 + 2)->len = 36;
    first->lines = lines1;

    file_state* second = (file_state*) malloc(sizeof(file_state));
    second->len = 3;
    file_line* lines2 = (file_line*)malloc(sizeof(file_line) * second->len);
    (lines2 + 0)->content = "this is the first line";
    (lines2 + 0)->len = 22;

    (lines2 + 1)->content = "this is the second line";
    (lines2 + 1)->len = 23;

    (lines2 + 2)->content = "this is the third and the final line";
    (lines2 + 2)->len = 36;
    second->lines = lines2;
    

    fdiff_t* diff = (fdiff_t*) malloc(sizeof(fdiff_t));
    diff_files(first, second, diff);
    ASSERT_EQ(0, diff->llen);
    PASS();
}

TEST diff_should_capture_new_added_lines_along_with_their_positions(void) {
    file_state* first = (file_state*) malloc(sizeof(file_state));
    first->len = 2;
    file_line* lines1 = (file_line*)malloc(sizeof(file_line) * first->len);
    (lines1 + 0)->content = "this is the first line";
    (lines1 + 0)->len = 22;

    (lines1 + 1)->content = "this is the second line";
    (lines1 + 1)->len = 23;

    first->lines = lines1;

    file_state* second = (file_state*) malloc(sizeof(file_state));
    second->len = 3;
    file_line* lines2 = (file_line*)malloc(sizeof(file_line) * second->len);
    (lines2 + 0)->content = "this is the first line";
    (lines2 + 0)->len = 22;

    (lines2 + 1)->content = "this is the second line";
    (lines2 + 1)->len = 23;

    (lines2 + 2)->content = "hello world";
    (lines2 + 2)->len = 11;

    second->lines = lines2;
    
    fdiff_t* diff = (fdiff_t*) malloc(sizeof(fdiff_t));
    diff_files(first, second, diff);
    ASSERT_EQ(1, diff->llen);
    ASSERT_EQ(2, diff->lines->num);
    ASSERT_EQ(11, diff->lines->len);
    ASSERT_STR_EQ("hello world", diff->lines->body);
    ASSERT_ENUM_EQ(DIFF_ADDED, diff->lines->kind, diff_kind_str);
    PASS();
}

TEST diff_should_capture_deleted_lines_along_with_their_positions(void) {
    file_state* first = (file_state*) malloc(sizeof(file_state));
    first->len = 3;
    file_line* lines1 = (file_line*)malloc(sizeof(file_line) * first->len);
    (lines1 + 0)->content = "this is the first line";
    (lines1 + 0)->len = 22;

    (lines1 + 1)->content = "this is the second line";
    (lines1 + 1)->len = 23;

    (lines1 + 2)->content = "this is the third line";
    (lines1 + 2)->len = 22;

    first->lines = lines1;

    file_state* second = (file_state*) malloc(sizeof(file_state));
    second->len = 2;
    file_line* lines2 = (file_line*)malloc(sizeof(file_line) * second->len);
    (lines2 + 0)->content = "this is the first line";
    (lines2 + 0)->len = 22;

    (lines2 + 1)->content = "this is the second line";
    (lines2 + 1)->len = 23;

    second->lines = lines2;
    
    fdiff_t* diff = (fdiff_t*) malloc(sizeof(fdiff_t));
    diff_files(first, second, diff);
    ASSERT_EQ(1, diff->llen);
    ASSERT_EQ(2, diff->lines->num);
    ASSERT_EQ(0, diff->lines->len);
    ASSERT_EQ(NULL, diff->lines->body);
    ASSERT_ENUM_EQ(DIFF_DELETED, diff->lines->kind, diff_kind_str);
    PASS();
}

TEST diff_should_capture_updated_lines_along_with_their_positions(void) {
    file_state* first = (file_state*) malloc(sizeof(file_state));
    first->len = 3;
    file_line* lines1 = (file_line*)malloc(sizeof(file_line) * first->len);
    (lines1 + 0)->content = "this is the first line";
    (lines1 + 0)->len = 22;

    (lines1 + 1)->content = "this is the second line";
    (lines1 + 1)->len = 23;

    (lines1 + 2)->content = "this is the third line";
    (lines1 + 2)->len = 22;

    first->lines = lines1;

    file_state* second = (file_state*) malloc(sizeof(file_state));
    second->len = 3;
    file_line* lines2 = (file_line*)malloc(sizeof(file_line) * second->len);
    (lines2 + 0)->content = "this is the first line";
    (lines2 + 0)->len = 22;

    (lines2 + 1)->content = "updated line";
    (lines2 + 1)->len = 12;

    (lines2 + 2)->content = "modified";
    (lines2 + 2)->len = 8;

    second->lines = lines2;
    
    fdiff_t* diff = (fdiff_t*) malloc(sizeof(fdiff_t));
    diff_files(first, second, diff);
    ASSERT_EQ(2, diff->llen);

    ASSERT_EQ(1, diff->lines->num);
    ASSERT_EQ(12, diff->lines->len);
    ASSERT_STR_EQ("updated line", diff->lines->body);
    ASSERT_ENUM_EQ(DIFF_MODIFIED, diff->lines->kind, diff_kind_str);


    ASSERT_EQ(2, (diff->lines + 1)->num);
    ASSERT_EQ(8, (diff->lines + 1)->len);
    ASSERT_STR_EQ("modified", (diff->lines + 1)->body);
    ASSERT_ENUM_EQ(DIFF_MODIFIED, (diff->lines + 1)->kind, diff_kind_str);
    PASS();
}

// test suite for diff.c
SUITE(diff_files_suite) {
    RUN_TEST(first_time_diff_should_contain_all_lines_of_kind_added);
    RUN_TEST(diff_called_on_identical_files_should_not_produce_any_diff);
    RUN_TEST(diff_should_capture_new_added_lines_along_with_their_positions);
    RUN_TEST(diff_should_capture_deleted_lines_along_with_their_positions);
    RUN_TEST(diff_should_capture_updated_lines_along_with_their_positions);
}


GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    // setup
    GREATEST_MAIN_BEGIN();

    // run suite
    RUN_SUITE(diff_files_suite);

    // results
    GREATEST_MAIN_END();
}