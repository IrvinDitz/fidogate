/*
 */

#include "rfc2ftn.h"
#include "prototypes.h"
#include "mock-log.h"
#include <cgreen/cgreen.h>
#include <cgreen/mocks.h>

int snd_mail(RFCAddr rfc_to, Textlist *body, long size);


Ensure(dummy)
{
    RFCAddr to = { 0 };
    Textlist body = { 0 };
    int r;
    long size = 1000;

    cgreen_mocks_are(loose_mocks);

    r = snd_mail(to, &body, size);
    assert_that(r, is_equal_to(0));
}

static void setup(void)
{
    log_buffer_release();
    debug_buffer_release();
}

TestSuite *create_snd_mail_suite(void)
{
    TestSuite *suite = create_named_test_suite("snd_mail suite");

    add_test(suite, dummy);

    set_setup(suite, setup);

    return suite;
}
