#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <ddcutil_c_api.h>
#include <ddcutil_status_codes.h>

#define GET 0
#define SET 1
#define BRIGHTNESS 0x10
#define CONTRAST 0x12
#define VOLUME 0x62

#define DDC_ERRMSG(function_name, status_code)     \
   do                                              \
   {                                               \
      printf("(%s) %s() returned %d (%s): %s\n",   \
             __func__, function_name, status_code, \
             ddca_rc_name(status_code),            \
             ddca_rc_desc(status_code));           \
   } while (0)

uint16_t m_action;
uint16_t m_target;
uint16_t m_value;

static char *shift_args(int *argc, char ***argv)
{
   assert(*argc > 0);
   char *result = **argv;
   *argc -= 1;
   *argv += 1;
   return result;
}

void usage(const char *program)
{
   fprintf(stderr, "Usage: %s <ACTION> <TARGET> [PERCENTAGE]\n", program);
   fprintf(stderr, "\nACTION (Required):\n");
   fprintf(stderr, "    get:           get a value\n");
   fprintf(stderr, "    set:           set a value\n");
   fprintf(stderr, "\nTARGET (Required):\n");
   fprintf(stderr, "    brightness:    get/set brightness\n");
   fprintf(stderr, "    contrast:      get/set contrast\n");
   fprintf(stderr, "    volume:        get/set volume\n");
   fprintf(stderr, "\nPERCENTAGE (Required for set ACTION):\n");
   fprintf(stderr, "    0 .. 100\n");
   fprintf(stderr, "\nExamples:\n");
   fprintf(stderr, "    $ %s get brightness\n", program);
   fprintf(stderr, "    $ %s set contrast 60\n", program);
}

int main(int argc, char **argv)
{
   const char *program = shift_args(&argc, &argv);
   if (argc == 0)
   {
      fprintf(stderr, "ERROR: no arguments provided\n");
      return 1;
   }

   while (argc > 0)
   {
      const char *action = shift_args(&argc, &argv);
      if (strcmp(action, "get") == 0)
      {
         m_action = GET;
         if (argc <= 0)
         {
            fprintf(stderr, "ERROR: no target is provided for `%s` action\n", action);
            return 1;
         }
      }
      else if (strcmp(action, "set") == 0)
      {
         m_action = SET;
         if (argc <= 0)
         {
            fprintf(stderr, "ERROR: no target is provided for `%s` action\n", action);
            return 1;
         }
      }
      else
      {
         usage(program);
         exit(0);
      }

      const char *target = shift_args(&argc, &argv);
      if (strcmp(target, "brightness") == 0)
      {
         m_target = BRIGHTNESS;
         if ((argc <= 0) && (m_action == SET))
         {
            fprintf(stderr, "ERROR: no value is provided for `%s` target\n", target);
            return 1;
         }
      }
      else if (strcmp(target, "contrast") == 0)
      {
         m_target = CONTRAST;
         if ((argc <= 0) && (m_action == SET))
         {
            fprintf(stderr, "ERROR: no value is provided for `%s` target\n", target);
            return 1;
         }
      }
      else if (strcmp(target, "volume") == 0)
      {
         m_target = VOLUME;
         if ((argc <= 0) && (m_action == SET))
         {
            fprintf(stderr, "ERROR: no value is provided for `%s` target\n", target);
            return 1;
         }
      }      
      else
      {
         usage(program);
         exit(0);
      }

      if (m_action == SET)
      {
         const char *value = shift_args(&argc, &argv);

         long lnum;
         char *end;

         lnum = strtol(value, &end, 10); // 10 specifies base-10
         if (end == value)
         { // if no characters were converted these pointers are equal
            fprintf(stderr, "ERROR: error parsing value for `%s` target\n", target);
            return 1;
         }
         if ((lnum < 0) || (lnum > 100))
         {
            fprintf(stderr, "ERROR: value out of range for `%s` target\n", target);
            return 1;
         }
         m_value = (uint16_t)lnum;
      }
   }
   DDCA_Status ok = 0;
   DDCA_Status rc;
   DDCA_Display_Ref dref;
   DDCA_Display_Handle dh = NULL; // initialize to avoid clang analyzer warning

   int MAX_DISPLAYS = 4; // limit the number of displays

   DDCA_Display_Info_List *dlist = NULL;
   ddca_get_display_info_list2(
       false, // don't include invalid displays
       &dlist);
   for (int ndx = 0; ndx < dlist->ct && ndx < MAX_DISPLAYS; ndx++)
   {
      DDCA_Display_Info *dinfo = &dlist->info[ndx];
      dref = dinfo->dref;
      rc = ddca_open_display2(dref, false, &dh);
      if (rc != 0)
      {
         DDC_ERRMSG("ddca_open_display", rc);
         ok = rc;
         continue;
      }
      if (m_action == GET)
      {
         DDCA_Status ddcrc0;
         DDCA_Non_Table_Vcp_Value valrec;
         ddcrc0 = ddca_get_non_table_vcp_value(
             dh,
             m_target,
             &valrec);
         if (ddcrc0 != 0)
         {
            DDC_ERRMSG("ddca_get_non_table_vcp_value", ddcrc0);
            ok = ddcrc0;
            continue;
         }
         uint16_t cur_val = valrec.sh << 8 | valrec.sl;
         fprintf(stdout, "%d\n", cur_val);
      }
      else if (m_action == SET)
      {
         uint8_t hi_byte = m_value >> 8;
         uint8_t lo_byte = m_value & 0xff;
         DDCA_Status ddcrc1 = ddca_set_non_table_vcp_value(dh, m_target, hi_byte, lo_byte);
         if (ddcrc1 == DDCRC_VERIFY)
         {
            fprintf(stderr, "ERROR: Value verification failed.\n");
            ok = ddcrc1;
            continue;
         }
         else if (ddcrc1 != 0)
         {
            DDC_ERRMSG("ddca_set_non_table_vcp_value", ddcrc1);
            ok = ddcrc1;
            continue;
         }
      }

      rc = ddca_close_display(dh);
      if (rc != 0) {
         DDC_ERRMSG("ddca_close_display", rc);
         ok = rc;
      }
      dh = NULL;
   }
   ddca_free_display_info_list(dlist);
   return ok;
}