/** \~english
 * \file constantCheckFunctionsMacros.h
 *
 * \author Massimo "Masstock" EYNARD
 * \version 0.5
 *
 * \brief Helping macros to write structures comparison functions.
 *
 * Checks are syntaxical structures starting with START_CHECK macro and closed
 * by END_CHECK macro building a expression equal to 1 if valid or 0 otherwise.
 * Checks allow to compare two structures (of the same definition) named
 * first and second.
 *
 * Grammar:
 *
 * expression -> #START_CHECK instructions_list #END_CHECK\n
 * instructions_list -> instruction instructions_list | &epsilon\n
 * instruction -> #EQUAL()\n
 * instruction -> #EXPR()\n
 * instruction -> #SUB_FUN()\n
 * instruction -> #SUB_FUN_PTR()\n
 * instruction -> #START_COND() instructions_list #END_COND()
 *
 * Example:
 * \code{.c}
 * typedef struct {
 *  bool infosPresent;
 *  struct {
 *    unsigned size;
 *    bool cbr;
 *  } infos;
 * } Test;
 *
 * ...
 * // In function body:
 * Test * first;
 * Test * second;
 *
 * bool result = START_CHECK
 *   EQUAL(->infosPresent)
 *   START_COND(->infosPresent, true)
 *     EQUAL(->infos.size)
 *     EQUAL(->infos.cbr)
 *   END_COND
 * END_CHECK;
 * \endcode
 */

#ifndef __LIBBLU_MUXER__CODECS__COMMON__CONSTANT_CHECK_FUN_MACROS_H__
#define __LIBBLU_MUXER__CODECS__COMMON__CONSTANT_CHECK_FUN_MACROS_H__

/** \~english
 * \brief Start a new Check expression.
 *
 */
#define START_CHECK  (1

/** \~english
 * \brief End the Check expression.
 *
 */
#define END_CHECK  )

/** \~english
 * \brief Check if both structures members are equal.
 *
 * \code{.c}
 * EQUAL(->frameRate)  // (first->frameRate == second->frameRate)
 * \endcode
 */
#define EQUAL(name)  && (first name == second name)

/** \~english
 * \brief Check if supplied expression is true.
 *
 */
#define EXPR(expr)  && (expr)

/** \~english
 * \brief Execute a check function on both structure members.
 *
 * Function pointer must be in the form of:
 *
 * a_type (function*) (b_type first, b_type second)
 *
 * Where: a_type is a integer constant (e.g. int or bool), b_type the struct
 * member type. The function must return a non-zero value as true.
 *
 * \code{.c}
 * SUB_FUN(->properties, checkProperties)
 *   // checkProperties(first->properties, second->properties)
 * \endcode
 */
#define SUB_FUN(name, fun)      && (fun(first name, second name))

/** \~english
 * \brief Execute a check function on pointers to both structure members.
 *
 * \code{.c}
 * SUB_FUN_PTR(->properties, checkProperties)
 *   // checkProperties(&first->properties, &second->properties)
 * \endcode
 */
#define SUB_FUN_PTR(name, fun)  && (fun(&first name, &second name))

/** \~english
 * \brief Start a condition sub-block.
 *
 * The sub-block is evaluated only if the structure member is equal to v. This
 * is similar as a if block, only accessed if condition is true.
 *
 * \note sub-block must be closed using #END_COND.
 *
 * \param name Structure member accessor. E.g. ".member" or "->member".
 * \param v Condition expected value.
 *
 * \code{.c}
 * EQUAL(->infosPresent) // Always ensure operands are identical
 * START_COND(->infosPresent, true)
 *   EQUAL(->infos.value)  // This check is only performed if
 *                         // (first->infosPresent == true)
 * END_COND
 * \endcode
 */
#define START_COND(name, v)     && ((v) != (first name) || (1

/** \~english
 * \brief Close a condition sub-block opened by #START_COND.
 *
 */
#define END_COND                ))

#endif